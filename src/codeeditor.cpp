#include "codeeditor.h"
#include "defines.h"

#include <QAction>
#include <QApplication>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QTextBlock>
#include <QWheelEvent>

#include <iterator>

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent) {
  m_lineNumberArea = new LineNumberArea(this);
  m_breakpointArea = new BreakpointArea(this);

  connect(this, SIGNAL(blockCountChanged(int)), this,
          SLOT(updateSidebarWidth(int)));

  connect(this, SIGNAL(updateRequest(QRect, int)), this,
          SLOT(updateSidebar(QRect, int)));

  // Connect runner PC to line hightlighting
  // connect(this, SIGNAL(cursorPositionChanged()), this,
  //        SLOT(highlightCurrentLine()));
  // highlightCurrentLine();

  updateSidebarWidth(0);

  // Set font for the entire widget. calls to fontMetrics() will get the
  // dimensions of the currently set font
  m_font = QFont("Monospace"); // set default font to Monospace on unix systems
  m_font.setStyleHint(QFont::Monospace);
  m_font.setPointSize(10);
  setFont(m_font);
  m_fontTimer.setSingleShot(true);

  // set event filter for catching scroll events
  installEventFilter(this);
}

int CodeEditor::lineNumberAreaWidth() {
  int digits = 1;
  int rightPadding = 6;
  int max = qMax(1, blockCount());
  while (max >= 10) {
    max /= 10;
    ++digits;
  }
  int space = rightPadding + fontMetrics().width(QLatin1Char('15')) * digits;
  return space;
}

void CodeEditor::updateSidebarWidth(int /* newBlockCount */) {
  // Set margins of the text edit area
  m_sidebarWidth = lineNumberAreaWidth() + m_breakpointArea->width();
  setViewportMargins(m_sidebarWidth, 0, 0, 0);
}

bool CodeEditor::eventFilter(QObject * /*observed*/, QEvent *event) {
  // Event filter for catching ctrl+Scroll events, for text resizing
  if (event->type() == QEvent::Wheel &&
      QApplication::keyboardModifiers() == Qt::ControlModifier) {
    auto wheelEvent = static_cast<QWheelEvent *>(event);
    // Since multiple wheelevents are issued on a scroll,
    // start a timer to only catch the first one

    // change font size
    if (!m_fontTimer.isActive()) {
      if (wheelEvent->angleDelta().y() > 0) {
        if (m_font.pointSize() < 30)
          m_font.setPointSize(m_font.pointSize() + 1);
      } else {
        if (m_font.pointSize() > 6)
          m_font.setPointSize(m_font.pointSize() - 1);
      }
      m_fontTimer.start(50);
    }
    setFont(m_font);
    return true;
  }

  return false;
}

void CodeEditor::updateSidebar(const QRect &rect, int dy) {
  if (dy) {
    m_lineNumberArea->scroll(0, dy);
    m_breakpointArea->scroll(0, dy);
  } else {
    m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(),
                             rect.height());
    m_breakpointArea->update(0, rect.y(), m_breakpointArea->width(),
                             rect.height());
  }

  if (rect.contains(viewport()->rect()))
    updateSidebarWidth(0);

  // Remove breakpoints if a breakpoint line has been removed
  while (!m_breakpoints.empty() &&
         *(m_breakpoints.rbegin()) > (blockCount() - 1)) {
    m_breakpoints.erase(std::prev(m_breakpoints.end()));
  }
}

void CodeEditor::resizeEvent(QResizeEvent *e) {
  QPlainTextEdit::resizeEvent(e);

  QRect cr = contentsRect();
  m_lineNumberArea->setGeometry(QRect(cr.left() + m_breakpointArea->width(),
                                      cr.top(), lineNumberAreaWidth(),
                                      cr.height()));

  m_breakpointArea->setGeometry(cr.left(), cr.top(), m_breakpointArea->width(),
                                cr.height());
}

void CodeEditor::highlightCurrentLine() {
  QList<QTextEdit::ExtraSelection> extraSelections;

  if (!isReadOnly()) {
    QTextEdit::ExtraSelection selection;

    QColor lineColor = QColor(Colors::Medalist).lighter(160);

    selection.format.setBackground(lineColor);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    extraSelections.append(selection);
  }

  setExtraSelections(extraSelections);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event) {
  QPainter painter(m_lineNumberArea);
  painter.fillRect(event->rect(), QColor(Qt::lightGray).lighter(120));

  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top = (int)blockBoundingGeometry(block).translated(contentOffset()).top();
  int bottom = top + (int)blockBoundingRect(block).height();

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      QString number = QString::number(blockNumber + 1);
      painter.setPen(QColor(Qt::gray).darker(130));
      painter.drawText(0, top, m_lineNumberArea->width() - 3,
                       fontMetrics().height(), Qt::AlignRight, number);
    }

    block = block.next();
    top = bottom;
    bottom = top + (int)blockBoundingRect(block).height();
    ++blockNumber;
  }
}

void CodeEditor::breakpointAreaPaintEvent(QPaintEvent *event) {
  QPainter painter(m_breakpointArea);

  // When caret flashes in QPlainTextEdit a paint event is sent to this widget,
  // with a height of a line in the edit. We override this paint event by always
  // redrawing the visible breakpoint area
  auto area = m_breakpointArea->rect();
  QLinearGradient gradient =
      QLinearGradient(area.topLeft(), area.bottomRight());
  gradient.setColorAt(0, QColor(Colors::FoundersRock).lighter(120));
  gradient.setColorAt(1, QColor(Colors::FoundersRock));
  painter.fillRect(area, gradient);

  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top = (int)blockBoundingGeometry(block).translated(contentOffset()).top();
  int bottom = top + (int)blockBoundingRect(block).height();

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      if (m_breakpoints.find(blockNumber) != m_breakpoints.end()) {
        painter.drawPixmap(
            m_breakpointArea->padding, top, m_breakpointArea->imageWidth,
            m_breakpointArea->imageHeight, m_breakpointArea->m_breakpoint);
      }
    }

    block = block.next();
    top = bottom;
    bottom = top + (int)blockBoundingRect(block).height();
    ++blockNumber;
  }
}

void CodeEditor::breakpointClick(QMouseEvent *event, int forceState) {

  // Get line height
  QTextBlock block = firstVisibleBlock();
  auto height = blockBoundingRect(block).height();

  // Find block index in the codeeditor
  int index;
  if (block == document()->findBlockByLineNumber(0)) {
    index = (event->pos().y() - contentOffset().y()) / height;
  } else {
    index = (event->pos().y() + contentOffset().y()) / height;
  }
  // Get actual block index
  while (index > 0) {
    block = block.next();
    index--;
  }
  // Set or unset breakpoint
  int blockNumber = block.blockNumber();
  if (block.isValid()) {
    auto brkptIter = m_breakpoints.find(blockNumber);
    // Set/unset breakpoint
    if (forceState == 1) {
      m_breakpoints.insert(blockNumber);
    } else if (forceState == 2) {
      if (brkptIter != m_breakpoints.end())
        m_breakpoints.erase(m_breakpoints.find(blockNumber));
    } else {
      if (brkptIter != m_breakpoints.end()) {
        m_breakpoints.erase(brkptIter);
      } else {
        m_breakpoints.insert(blockNumber);
      }
    }
    repaint();
  }
}

// -------------- breakpoint area ----------------------------------

BreakpointArea::BreakpointArea(CodeEditor *editor) : QWidget(editor) {
  codeEditor = editor;
  setCursor(Qt::PointingHandCursor);

  // Create and connect actions for removing and setting breakpoints
  m_removeAction = new QAction("Remove breakpoint", this);
  m_removeAllAction = new QAction("Remove all breakpoints", this);
  m_addAction = new QAction("Add breakpoint", this);

  connect(m_removeAction, &QAction::triggered,
          [=] { codeEditor->breakpointClick(m_event, 2); });
  connect(m_addAction, &QAction::triggered,
          [=] { codeEditor->breakpointClick(m_event, 1); });
  connect(m_removeAllAction, &QAction::triggered, [=] {
    codeEditor->clearBreakpoints();
    repaint();
  });

  // Construct default mouseButtonEvent
  m_event = new QMouseEvent(QEvent::MouseButtonRelease, QPoint(0, 0),
                            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
}

void BreakpointArea::contextMenuEvent(QContextMenuEvent *event) {
  // setup context menu
  QMenu contextMenu;

  // Translate event to a QMouseEvent in case add/remove single breakpoint is
  // triggered
  *m_event = QMouseEvent(QEvent::MouseButtonRelease, event->pos(),
                         Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);

  contextMenu.addAction(m_addAction);
  contextMenu.addAction(m_removeAction);
  contextMenu.addAction(m_removeAllAction);

  contextMenu.exec(event->globalPos());
}