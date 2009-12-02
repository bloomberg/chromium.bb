// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/browser_bubble.h"

#include "app/l10n_util_win.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_win.h"
#include "views/window/window.h"

class BubbleWidget : public views::WidgetWin {
 public:
  explicit BubbleWidget(BrowserBubble* bubble)
      : bubble_(bubble), closed_(false) {
    set_window_style(WS_POPUP | WS_CLIPCHILDREN);
    set_window_ex_style(WS_EX_TOOLWINDOW);
  }

  void Show(bool activate) {
    if (activate)
      ShowWindow(SW_SHOW);
    else
      views::WidgetWin::Show();
  }

  void Close() {
    if (closed_)
      return;
    closed_ = true;
    if (IsActive()) {
      BrowserBubble::Delegate* delegate = bubble_->delegate();
      if (delegate)
        delegate->BubbleLostFocus(bubble_, NULL);
    }
    views::WidgetWin::Close();
  }

  void Hide() {
    if (IsActive()) {
      BrowserBubble::Delegate* delegate = bubble_->delegate();
      if (delegate)
        delegate->BubbleLostFocus(bubble_, NULL);
    }
    views::WidgetWin::Hide();
  }

  void OnActivate(UINT action, BOOL minimized, HWND window) {
    BrowserBubble::Delegate* delegate = bubble_->delegate();
    if (!delegate) {
      if (action == WA_INACTIVE && !closed_) {
        bubble_->DetachFromBrowser();
        delete bubble_;
      }
      return;
    }

    if (action == WA_INACTIVE && !closed_) {
      delegate->BubbleLostFocus(bubble_, window);
    }
  }

  virtual void OnSetFocus(HWND focused_window) {
    BrowserBubble::Delegate* delegate = bubble_->delegate();
    if (delegate)
      delegate->BubbleGotFocus(bubble_);
  }

 private:
  bool closed_;
  BrowserBubble* bubble_;
};

void BrowserBubble::InitPopup() {
  // popup_ is a Widget, but we need to do some WidgetWin stuff first, then
  // we'll assign it into popup_.
  views::WidgetWin* pop = new BubbleWidget(this);
  pop->Init(frame_native_view_, bounds_);
  pop->SetContentsView(view_);

  popup_ = pop;
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
  pop->Show(activate);
  visible_ = true;
}

void BrowserBubble::Hide() {
  if (!visible_)
    return;
  views::WidgetWin* pop = static_cast<views::WidgetWin*>(popup_);
  pop->Hide();
  visible_ = false;
}
