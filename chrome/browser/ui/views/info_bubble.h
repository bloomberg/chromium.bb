// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFO_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_INFO_BUBBLE_H_
#pragma once

#include "chrome/browser/ui/views/bubble_border.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/animation/animation_delegate.h"
#include "views/accelerator.h"
#include "views/view.h"

#if defined(OS_WIN)
#include "views/widget/native_widget_win.h"
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

namespace gfx {
class Path;
}

namespace ui {
class SlideAnimation;
}

namespace views {
class Widget;
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

  // Returns the bounds for the monitor showing the specified |rect|.
  // Overridden in unit-tests.
  virtual gfx::Rect GetMonitorBounds(const gfx::Rect& rect);

  // Margins between the contents and the inside of the border, in pixels.
  static const int kLeftMargin = 6;
  static const int kTopMargin = 6;
  static const int kRightMargin = 6;
  static const int kBottomMargin = 9;

  BubbleBorder* bubble_border_;

 private:
  // Overridden from View:
  virtual void Paint(gfx::Canvas* canvas);

  // Changes |arrow_location| to its mirrored version, vertically if |vertical|
  // is true, horizontally otherwise, if |window_bounds| don't fit in
  // |monitor_bounds|.
  void MirrorArrowIfOffScreen(bool vertical,
                              const gfx::Rect& position_relative_to,
                              const gfx::Rect& monitor_bounds,
                              const gfx::Size& local_contents_size,
                              BubbleBorder::ArrowLocation* arrow_location,
                              gfx::Rect* window_bounds);

  // Computes how much |window_bounds| is off-screen of the monitor bounds
  // |monitor_bounds| and puts the values in |offscreen_insets|.
  // Returns false if |window_bounds| is actually contained in |monitor_bounds|,
  // in which case |offscreen_insets| is not modified.
  static bool ComputeOffScreenInsets(const gfx::Rect& monitor_bounds,
                                     const gfx::Rect& window_bounds,
                                     gfx::Insets* offscreen_insets);

  // Convenience methods that returns the height of |insets| if |vertical| is
  // true, its width otherwise.
  static int GetInsetsLength(const gfx::Insets& insets, bool vertical);

  DISALLOW_COPY_AND_ASSIGN(BorderContents);
};

#if defined(OS_WIN)
// This is a window that surrounds the info bubble and paints the margin and
// border.  It is a separate window so that it can be a layered window, so that
// we can use >1-bit alpha shadow images on the borders, which look nicer than
// the Windows CS_DROPSHADOW shadows.  The info bubble window itself cannot be a
// layered window because that prevents it from hosting native child controls.
class BorderWidget : public views::NativeWidgetWin {
 public:
  BorderWidget();
  virtual ~BorderWidget() { }

  // Initializes the BrowserWidget making |owner| its owning window.
  void Init(BorderContents* border_contents, HWND owner);

  // Given the size of the contained contents (without margins), and the rect
  // (in screen coordinates) to point to, sets the border window positions and
  // sizes the border window and returns the bounds (in screen coordinates) the
  // contents should use. |arrow_location| is prefered arrow location,
  // the function tries to preserve the location and direction, in case of RTL
  // arrow location is mirrored.
  virtual gfx::Rect SizeAndGetBounds(const gfx::Rect& position_relative_to,
                                     BubbleBorder::ArrowLocation arrow_location,
                                     const gfx::Size& contents_size);

  // Simple accessors.
  BorderContents* border_contents() { return border_contents_; }

 protected:
  BorderContents* border_contents_;

 private:
  // Overridden from NativeWidgetWin:
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

  // Whether the InfoBubble should fade in when opening. When trying to
  // determine whether to use FadeIn, consider whether the bubble is shown as a
  // direct result of a user action or not. For example, if the bubble is being
  // shown as a direct result of a mouse-click, we should not use FadeIn.
  // However, if the bubble appears as a notification that something happened
  // in the background, we use FadeIn.
  virtual bool FadeInOnShow() = 0;

  // The name of the window to which this delegate belongs.
  virtual std::wstring accessible_name() { return L""; }
};

// TODO(sky): this code is ifdef-tastic. It might be cleaner to refactor the
// WidgetFoo subclass into a separate class that calls into InfoBubble.
// That way InfoBubble has no (or very few) ifdefs.
class InfoBubble
#if defined(OS_WIN)
    : public views::NativeWidgetWin,
#elif defined(OS_LINUX)
    : public views::WidgetGtk,
#endif
      public views::AcceleratorTarget,
      public ui::AnimationDelegate {
 public:
  // Shows the InfoBubble.  |parent| is set as the parent window, |contents| are
  // the contents shown in the bubble, and |position_relative_to| is a rect in
  // screen coordinates at which the InfoBubble will point.  Show() takes
  // ownership of |contents| and deletes the created InfoBubble when another
  // window is activated.  You can explicitly close the bubble by invoking
  // Close(). |arrow_location| specifies preferred bubble alignment.
  // You may provide an optional |delegate| to:
  //     - Be notified when the InfoBubble is closed.
  //     - Prevent the InfoBubble from being closed when the Escape key is
  //       pressed (the default behavior).
  static InfoBubble* Show(views::Widget* parent,
                          const gfx::Rect& position_relative_to,
                          BubbleBorder::ArrowLocation arrow_location,
                          views::View* contents,
                          InfoBubbleDelegate* delegate);

#if defined(OS_CHROMEOS)
  // Shows the InfoBubble without grabbing the focus. Others are the same as
  // above.  TYPE_POPUP widget is used to achieve the focusless effect.
  // If |show_while_screen_is_locked| is true, a property is set telling the
  // window manager to continue showing the bubble even while the screen is
  // locked.
  static InfoBubble* ShowFocusless(views::Widget* parent,
                                   const gfx::Rect& position_relative_to,
                                   BubbleBorder::ArrowLocation arrow_location,
                                   views::View* contents,
                                   InfoBubbleDelegate* delegate,
                                   bool show_while_screen_is_locked);
#endif

  // Resizes and potentially moves the InfoBubble to best accommodate the
  // contents preferred size.
  void SizeToContents();

  // Whether the InfoBubble should fade away when it closes. Generally speaking,
  // we use FadeOut when the user selects something within the bubble that
  // causes the bubble to dismiss. We don't use it when the bubble gets
  // deactivated as a result of clicking outside the bubble.
  void set_fade_away_on_close(bool fade_away_on_close) {
    fade_away_on_close_ = fade_away_on_close;
  }

  // Overridden from NativeWidgetWin:
  virtual void Close();

  // Overridden from ui::AnimationDelegate:
  virtual void AnimationEnded(const ui::Animation* animation);
  virtual void AnimationProgressed(const ui::Animation* animation);

  static const SkColor kBackgroundColor;

 protected:
  InfoBubble();
#if defined(OS_CHROMEOS)
  InfoBubble(views::WidgetGtk::Type type, bool show_while_screen_is_locked);
#endif
  virtual ~InfoBubble();

  // Creates the InfoBubble.
  virtual void Init(views::Widget* parent,
                    const gfx::Rect& position_relative_to,
                    BubbleBorder::ArrowLocation arrow_location,
                    views::View* contents,
                    InfoBubbleDelegate* delegate);

  // Instantiates and returns the BorderContents this InfoBubble should use.
  // Subclasses can return their own BorderContents implementation.
  virtual BorderContents* CreateBorderContents();

#if defined(OS_WIN)
  // Overridden from NativeWidgetWin:
  virtual void OnActivate(UINT action, BOOL minimized, HWND window);
#elif defined(OS_LINUX)
  // Overridden from WidgetGtk:
  virtual void IsActiveChanged();
#endif

#if defined(OS_WIN)
  // The window used to render the padding, border and arrow.
  BorderWidget* border_;
#elif defined(OS_LINUX)
  // The view displaying the border.
  BorderContents* border_contents_;
#endif

 private:
  enum ShowStatus {
    kOpen,
    kClosing,
    kClosed
  };

  // Closes the window notifying the delegate. |closed_by_escape| is true if
  // the close is the result of pressing escape.
  void DoClose(bool closed_by_escape);

  // Animates to a visible state.
  void FadeIn();
  // Animates to a hidden state.
  void FadeOut();

  // Animates to a visible/hidden state (visible if |fade_in| is true).
  void Fade(bool fade_in);

  // Overridden from AcceleratorTarget:
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  // The delegate, if any.
  InfoBubbleDelegate* delegate_;

  // The animation used to fade the bubble out.
  scoped_ptr<ui::SlideAnimation> animation_;

  // The current visibility status of the bubble.
  ShowStatus show_status_;

  // Whether to fade away when the bubble closes.
  bool fade_away_on_close_;

#if defined(OS_CHROMEOS)
  // Should we set a property telling the window manager to show this window
  // onscreen even when the screen is locked?
  bool show_while_screen_is_locked_;
#endif

  gfx::Rect position_relative_to_;
  BubbleBorder::ArrowLocation arrow_location_;

  views::View* contents_;

  DISALLOW_COPY_AND_ASSIGN(InfoBubble);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFO_BUBBLE_H_
