// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_INFO_BUBBLE_H_
#define CHROME_BROWSER_VIEWS_INFO_BUBBLE_H_

#include "third_party/skia/include/core/SkColor.h"
#include "views/accelerator.h"
#include "views/view.h"
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

#if defined(OS_WIN)
class BorderWidget;
#endif
class InfoBubble;

namespace views {
class Widget;
}

namespace gfx {
class Path;
}

// This is used to paint the border of the InfoBubble.  Windows uses this via
// BorderWidget (see below), while others can use it directly in the bubble.
class BorderContents : public views::View {
 public:
  BorderContents() : bubble_border_(NULL) { }

  // Must be called before this object can be used.
  void Init();

  // Given the size of the contents and the rect to point at, returns the bounds
  // of both the border and the contents inside the bubble.
  // |arrow_location| specifies the preferred location for the arrow
  // anchor. If the bubble does not fit on the monitor and
  // |allow_bubble_offscreen| is false, the arrow location may change so the
  // bubble shows entirely.
  virtual void SizeAndGetBounds(
      const gfx::Rect& position_relative_to,  // In screen coordinates
      BubbleBorder::ArrowLocation arrow_location,
      bool allow_bubble_offscreen,
      const gfx::Size& contents_size,
      gfx::Rect* contents_bounds,             // Returned in window coordinates
      gfx::Rect* window_bounds);              // Returned in screen coordinates

 protected:
  virtual ~BorderContents() { }

  // Margins between the contents and the inside of the border, in pixels.
  static const int kLeftMargin = 6;
  static const int kTopMargin = 6;
  static const int kRightMargin = 6;
  static const int kBottomMargin = 9;

  BubbleBorder* bubble_border_;

 private:
  // Overridden from View:
  virtual void Paint(gfx::Canvas* canvas);

  DISALLOW_COPY_AND_ASSIGN(BorderContents);
};

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

  // Initializes the BrowserWidget making |owner| its owning window.
  void Init(HWND owner);

  // Given the size of the contained contents (without margins), and the rect
  // (in screen coordinates) to point to, sets the border window positions and
  // sizes the border window and returns the bounds (in screen coordinates) the
  // contents should use. |arrow_location| is prefered arrow location,
  // the function tries to preserve the location and direction, in case of RTL
  // arrow location is mirrored.
  virtual gfx::Rect SizeAndGetBounds(const gfx::Rect& position_relative_to,
                                     BubbleBorder::ArrowLocation arrow_location,
                                     const gfx::Size& contents_size);

 protected:
  // Instanciates and returns the BorderContents this BorderWidget should use.
  // Subclasses can return their own BorderContents implementation.
  virtual BorderContents* CreateBorderContents();

  BorderContents* border_contents_;

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

// TODO(sky): this code is ifdef-tastic. It might be cleaner to refactor the
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
  // Close(). |arrow_location| specifies prefered bubble alignment.
  // You may provide an optional |delegate| to:
  //     - Be notified when the InfoBubble is closed.
  //     - Prevent the InfoBubble from being closed when the Escape key is
  //       pressed (the default behavior).
  static InfoBubble* Show(views::Widget* parent,
                          const gfx::Rect& position_relative_to,
                          BubbleBorder::ArrowLocation arrow_location,
                          views::View* contents,
                          InfoBubbleDelegate* delegate);

  // Resizes and potentially moves the InfoBubble to best accomodate the
  // contents preferred size.
  void SizeToContents();

  // Overridden from WidgetWin:
  virtual void Close();

  static const SkColor kBackgroundColor;

 protected:
  InfoBubble();
  virtual ~InfoBubble() {}

  // Creates the InfoBubble.
  virtual void Init(views::Widget* parent,
                    const gfx::Rect& position_relative_to,
                    BubbleBorder::ArrowLocation arrow_location,
                    views::View* contents,
                    InfoBubbleDelegate* delegate);

#if defined(OS_WIN)
  // Instanciates and returns the BorderWidget this InfoBubble should use.
  // Subclasses can return their own BorderWidget specialization.
  virtual BorderWidget* CreateBorderWidget();
#endif

#if defined(OS_WIN)
  // Overridden from WidgetWin:
  virtual void OnActivate(UINT action, BOOL minimized, HWND window);
#elif defined(OS_LINUX)
  // Overridden from WidgetGtk:
  virtual void IsActiveChanged();
#endif

#if defined(OS_WIN)
  // The window used to render the padding, border and arrow.
  scoped_ptr<BorderWidget> border_;
#elif defined(OS_LINUX)
  // The view displaying the border.
  BorderContents* border_contents_;
#endif

 private:
  // Closes the window notifying the delegate. |closed_by_escape| is true if
  // the close is the result of pressing escape.
  void Close(bool closed_by_escape);

  // Overridden from AcceleratorTarget:
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  // The delegate, if any.
  InfoBubbleDelegate* delegate_;

  // Have we been closed?
  bool closed_;

  gfx::Rect position_relative_to_;
  BubbleBorder::ArrowLocation arrow_location_;

  views::View* contents_;


  DISALLOW_COPY_AND_ASSIGN(InfoBubble);
};

#endif  // CHROME_BROWSER_VIEWS_INFO_BUBBLE_H_
