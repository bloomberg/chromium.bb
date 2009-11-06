// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_INFO_BUBBLE_H_
#define CHROME_BROWSER_VIEWS_INFO_BUBBLE_H_

#include "chrome/browser/views/bubble_border.h"

#if defined(OS_WIN)
#include "views/widget/widget_win.h"
#elif defined(OS_LINUX)
#include "views/widget/widget_gtk.h"
#endif

// InfoBubble is used to display an arbitrary view above all other windows.
// Think of InfoBubble as a tooltip that allows you to embed an arbitrary view
// in the tooltip. Additionally the InfoBubble renders an arrow pointing at
// the region the info bubble is providing the information about.
//
// To use an InfoBubble, invoke Show() and it'll take care of the rest.  The
// InfoBubble insets the contents for you, so the contents typically shouldn't
// have any additional margins.

class BorderWidget;
class InfoBubble;

namespace views {
class Window;
}

namespace gfx {
class Path;
}

#if defined(OS_WIN)
// This is a window that surrounds the info bubble and paints the margin and
// border.  It is a separate window so that it can be a layered window, so that
// we can use >1-bit alpha shadow images on the borders, which look nicer than
// the Windows CS_DROPSHADOW shadows.  The info bubble window itself cannot be a
// layered window because that prevents it from hosting native child controls.
class BorderWidget : public views::WidgetWin {
 public:
  BorderWidget();
  virtual ~BorderWidget() { }

  // Given the owning (parent) window, the size of the contained contents
  // (without margins), and the rect (in screen coordinates) to point to,
  // initializes the window and returns the bounds (in screen coordinates) the
  // contents should use.  |is_rtl| is supplied to
  // BorderContents::InitAndGetBounds(), see its declaration for details.
  gfx::Rect InitAndGetBounds(HWND owner,
                             const gfx::Rect& position_relative_to,
                             const gfx::Size& contents_size,
                             bool is_rtl);

 private:
  // Overridden from WidgetWin:
  virtual LRESULT OnMouseActivate(HWND window,
                                  UINT hit_test,
                                  UINT mouse_message);

  DISALLOW_COPY_AND_ASSIGN(BorderWidget);
};
#endif

class InfoBubbleDelegate {
 public:
  // Called when the InfoBubble is closing and is about to be deleted.
  // |closed_by_escape| is true if the close is the result of the user pressing
  // escape.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape) = 0;

  // Whether the InfoBubble should be closed when the Esc key is pressed.
  virtual bool CloseOnEscape() = 0;
};

// TODO: this code is ifdef-tastic. It might be cleaner to refactor the
// WidgetFoo subclass into a separate class that calls into InfoBubble.
// That way InfoBubble has no (or very few) ifdefs.
class InfoBubble
#if defined(OS_WIN)
    : public views::WidgetWin,
#elif defined(OS_LINUX)
    : public views::WidgetGtk,
#endif
      public views::AcceleratorTarget {
 public:
  // Shows the InfoBubble.  |parent| is set as the parent window, |contents| are
  // the contents shown in the bubble, and |position_relative_to| is a rect in
  // screen coordinates at which the InfoBubble will point.  Show() takes
  // ownership of |contents| and deletes the created InfoBubble when another
  // window is activated.  You can explicitly close the bubble by invoking
  // Close().  You may provide an optional |delegate| to be notified when the
  // InfoBubble is closed and/or to prevent the InfoBubble from being closed
  // when the Escape key is pressed (the default behavior).
  static InfoBubble* Show(views::Window* parent,
                          const gfx::Rect& position_relative_to,
                          views::View* contents,
                          InfoBubbleDelegate* delegate);

  // Overridden from WidgetWin:
  virtual void Close();

  static const SkColor kBackgroundColor;

 protected:
  InfoBubble();
  virtual ~InfoBubble() {}

  // Creates the InfoBubble.
  void Init(views::Window* parent,
            const gfx::Rect& position_relative_to,
            views::View* contents,
            InfoBubbleDelegate* delegate);

#if defined(OS_WIN)
  // Overridden from WidgetWin:
  virtual void OnActivate(UINT action, BOOL minimized, HWND window);
#elif defined(OS_LINUX)
  // Overridden from WidgetGtk:
  virtual void IsActiveChanged();
#endif

 private:
  // Closes the window notifying the delegate. |closed_by_escape| is true if
  // the close is the result of pressing escape.
  void Close(bool closed_by_escape);

  // Overridden from AcceleratorTarget:
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  // The delegate, if any.
  InfoBubbleDelegate* delegate_;

  // The window that this InfoBubble is parented to.
  views::Window* parent_;

#if defined(OS_WIN)
  // The window used to render the padding, border and arrow.
  scoped_ptr<BorderWidget> border_;
#endif

  // Have we been closed?
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(InfoBubble);
};

#endif  // CHROME_BROWSER_VIEWS_INFO_BUBBLE_H_
