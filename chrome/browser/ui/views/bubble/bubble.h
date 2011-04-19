// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_BUBBLE_H_
#pragma once

#include "chrome/browser/ui/views/bubble/bubble_border.h"
#include "ui/base/animation/animation_delegate.h"
#include "views/accelerator.h"
#include "views/view.h"

#if defined(OS_WIN)
#include "views/widget/widget_win.h"
#elif defined(OS_LINUX)
#include "views/widget/widget_gtk.h"
#endif

// Bubble is used to display an arbitrary view above all other windows.
// Think of Bubble as a tooltip that allows you to embed an arbitrary view
// in the tooltip. Additionally the Bubble renders an arrow pointing at
// the region the info bubble is providing the information about.
//
// To use an Bubble, invoke Show() and it'll take care of the rest.  The Bubble
// insets the contents for you, so the contents typically shouldn't have any
// additional margins.

class BorderContents;
#if defined(OS_WIN)
class BorderWidgetWin;
#endif
class Bubble;

namespace gfx {
class Path;
}

namespace ui {
class SlideAnimation;
}

namespace views {
class Widget;
}

class BubbleDelegate {
 public:
  // Called when the Bubble is closing and is about to be deleted.
  // |closed_by_escape| is true if the close is the result of the user pressing
  // escape.
  virtual void BubbleClosing(Bubble* bubble, bool closed_by_escape) = 0;

  // Whether the Bubble should be closed when the Esc key is pressed.
  virtual bool CloseOnEscape() = 0;

  // Whether the Bubble should fade in when opening. When trying to determine
  // whether to use FadeIn, consider whether the bubble is shown as a direct
  // result of a user action or not. For example, if the bubble is being shown
  // as a direct result of a mouse-click, we should not use FadeIn. However, if
  // the bubble appears as a notification that something happened in the
  // background, we use FadeIn.
  virtual bool FadeInOnShow() = 0;

  // The name of the window to which this delegate belongs.
  virtual std::wstring accessible_name();
};

// TODO(sky): this code is ifdef-tastic. It might be cleaner to refactor the
// WidgetFoo subclass into a separate class that calls into Bubble.
// That way Bubble has no (or very few) ifdefs.
class Bubble
#if defined(OS_WIN)
    : public views::WidgetWin,
#elif defined(OS_LINUX)
    : public views::WidgetGtk,
#endif
      public views::AcceleratorTarget,
      public ui::AnimationDelegate {
 public:
  // Shows the Bubble.
  // |parent| is set as the parent window.
  // |contents| are the contents shown in the bubble.
  // |position_relative_to| is a rect in screen coordinates at which the Bubble
  // will point.
  // Show() takes ownership of |contents| and deletes the created Bubble when
  // another window is activated. You can explicitly close the bubble by
  // invoking Close().
  // |arrow_location| specifies preferred bubble alignment.
  // You may provide an optional |delegate| to:
  //     - Be notified when the Bubble is closed.
  //     - Prevent the Bubble from being closed when the Escape key is
  //       pressed (the default behavior).
  static Bubble* Show(views::Widget* parent,
                      const gfx::Rect& position_relative_to,
                      BubbleBorder::ArrowLocation arrow_location,
                      views::View* contents,
                      BubbleDelegate* delegate);

#if defined(OS_CHROMEOS)
  // Shows the Bubble without grabbing the focus. Others are the same as
  // above.  TYPE_POPUP widget is used to achieve the focusless effect.
  // If |show_while_screen_is_locked| is true, a property is set telling the
  // window manager to continue showing the bubble even while the screen is
  // locked.
  static Bubble* ShowFocusless(views::Widget* parent,
                               const gfx::Rect& position_relative_to,
                               BubbleBorder::ArrowLocation arrow_location,
                               views::View* contents,
                               BubbleDelegate* delegate,
                               bool show_while_screen_is_locked);
#endif

  // Resizes and potentially moves the Bubble to best accommodate the
  // contents preferred size.
  void SizeToContents();

  // Whether the Bubble should fade away when it closes. Generally speaking,
  // we use FadeOut when the user selects something within the bubble that
  // causes the bubble to dismiss. We don't use it when the bubble gets
  // deactivated as a result of clicking outside the bubble.
  void set_fade_away_on_close(bool fade_away_on_close) {
    fade_away_on_close_ = fade_away_on_close;
  }

  // Overridden from WidgetWin:
  virtual void Close();

  // Overridden from ui::AnimationDelegate:
  virtual void AnimationEnded(const ui::Animation* animation);
  virtual void AnimationProgressed(const ui::Animation* animation);

  static const SkColor kBackgroundColor;

 protected:
  Bubble();
#if defined(OS_CHROMEOS)
  Bubble(views::WidgetGtk::Type type, bool show_while_screen_is_locked);
#endif
  virtual ~Bubble();

  // Creates the Bubble.
  virtual void InitBubble(views::Widget* parent,
                          const gfx::Rect& position_relative_to,
                          BubbleBorder::ArrowLocation arrow_location,
                          views::View* contents,
                          BubbleDelegate* delegate);

  // Instantiates and returns the BorderContents this Bubble should use.
  // Subclasses can return their own BorderContents implementation.
  virtual BorderContents* CreateBorderContents();

#if defined(OS_WIN)
  // Overridden from WidgetWin:
  virtual void OnActivate(UINT action, BOOL minimized, HWND window);
#elif defined(OS_LINUX)
  // Overridden from WidgetGtk:
  virtual void IsActiveChanged();
#endif

#if defined(OS_WIN)
  // The window used to render the padding, border and arrow.
  BorderWidgetWin* border_;
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
  BubbleDelegate* delegate_;

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

  DISALLOW_COPY_AND_ASSIGN(Bubble);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_BUBBLE_H_
