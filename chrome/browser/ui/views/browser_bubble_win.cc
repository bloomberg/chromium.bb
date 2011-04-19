// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/browser_bubble.h"

#include "chrome/browser/ui/views/bubble/border_contents.h"
#include "chrome/browser/ui/views/bubble/border_widget_win.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_win.h"
#include "views/window/window.h"

class BubbleWidget : public views::WidgetWin {
 public:
  explicit BubbleWidget(BrowserBubble* bubble)
      : bubble_(bubble),
        border_widget_(new BorderWidgetWin) {
    set_window_style(WS_POPUP | WS_CLIPCHILDREN);
    set_window_ex_style(WS_EX_TOOLWINDOW);
  }

  void ShowAndActivate(bool activate) {
    // Show the border first, then the popup overlaid on top.
    border_widget_->Show();
    if (activate)
      ShowWindow(SW_SHOW);
    else
      views::WidgetWin::Show();
  }

  void Close() {
    if (!bubble_)
      return;  // We have already been closed.
    if (IsActive()) {
      BrowserBubble::Delegate* delegate = bubble_->delegate();
      if (delegate)
        delegate->BubbleLostFocus(bubble_, NULL);
    }
    border_widget_->Close();
    views::WidgetWin::Close();
    bubble_ = NULL;
  }

  void Hide() {
    if (IsActive() && bubble_) {
      BrowserBubble::Delegate* delegate = bubble_->delegate();
      if (delegate)
        delegate->BubbleLostFocus(bubble_, NULL);
    }
    views::WidgetWin::Hide();
    border_widget_->Hide();
  }

  void OnActivate(UINT action, BOOL minimized, HWND window) {
    WidgetWin::OnActivate(action, minimized, window);
    if (!bubble_)
      return;

    BrowserBubble::Delegate* delegate = bubble_->delegate();
    if (!delegate) {
      if (action == WA_INACTIVE) {
        bubble_->DetachFromBrowser();
        delete bubble_;
      }
      return;
    }

    if (action == WA_INACTIVE) {
      bool lost_focus_to_child = false;

      // Are we a parent of this window?
      gfx::NativeView parent = window;
      while (parent = ::GetParent(parent)) {
        if (window == GetNativeView()) {
          lost_focus_to_child = true;
          break;
        }
      }

      // Do we own this window?
      if (!lost_focus_to_child &&
          ::GetWindow(window, GW_OWNER) == GetNativeView()) {
        lost_focus_to_child = true;
      }

      delegate->BubbleLostFocus(bubble_, lost_focus_to_child);
    }
  }

  virtual void OnSetFocus(HWND focused_window) {
    WidgetWin::OnSetFocus(focused_window);
    if (bubble_ && bubble_->delegate())
      bubble_->delegate()->BubbleGotFocus(bubble_);
  }

  BorderWidgetWin* border_widget() {
    return border_widget_;
  }

 private:
  BrowserBubble* bubble_;
  BorderWidgetWin* border_widget_;

  DISALLOW_COPY_AND_ASSIGN(BubbleWidget);
};

void BrowserBubble::InitPopup() {
  // popup_ is a Widget, but we need to do some WidgetWin stuff first, then
  // we'll assign it into popup_.
  BubbleWidget* pop = new BubbleWidget(this);

  BorderWidgetWin* border_widget = pop->border_widget();
  border_widget->Init(new BorderContents, frame_->GetNativeView());

  // We make the BorderWidgetWin the owner of the Bubble HWND, so that the
  // latter is displayed on top of the former.
  pop->Init(border_widget->GetNativeView(), gfx::Rect());
  pop->SetContentsView(view_);

  popup_ = pop;

  ResizeToView();
  Reposition();
  AttachToBrowser();
}

void BrowserBubble::MovePopup(int x, int y, int w, int h) {
  views::WidgetWin* pop = static_cast<views::WidgetWin*>(popup_);
  pop->SetBounds(gfx::Rect(x, y, w, h));
}

void BrowserBubble::Show(bool activate) {
  if (visible_)
    return;
  BubbleWidget* pop = static_cast<BubbleWidget*>(popup_);
  pop->ShowAndActivate(activate);
  visible_ = true;
}

void BrowserBubble::Hide() {
  if (!visible_)
    return;
  views::WidgetWin* pop = static_cast<views::WidgetWin*>(popup_);
  pop->Hide();
  visible_ = false;
}

void BrowserBubble::ResizeToView() {
  BorderWidgetWin* border_widget =
      static_cast<BubbleWidget*>(popup_)->border_widget();

  gfx::Rect window_bounds;
  window_bounds = border_widget->SizeAndGetBounds(GetAbsoluteRelativeTo(),
      arrow_location_, view_->size());

  SetAbsoluteBounds(window_bounds);
}
