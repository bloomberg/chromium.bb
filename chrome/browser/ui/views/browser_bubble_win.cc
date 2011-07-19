// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/browser_bubble.h"

#include "chrome/browser/ui/views/bubble/border_contents.h"
#include "chrome/browser/ui/views/bubble/border_widget_win.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "views/widget/root_view.h"
#include "views/widget/native_widget_win.h"
#include "views/widget/widget.h"

class BubbleWidget : public views::NativeWidgetWin {
 public:
  explicit BubbleWidget(BrowserBubble* bubble)
      : views::NativeWidgetWin(new views::Widget),
        bubble_(bubble),
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
      views::NativeWidgetWin::Show();
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
    views::NativeWidgetWin::Close();
    bubble_ = NULL;
  }

  void Hide() {
    if (IsActive() && bubble_) {
      BrowserBubble::Delegate* delegate = bubble_->delegate();
      if (delegate)
        delegate->BubbleLostFocus(bubble_, NULL);
    }
    views::NativeWidgetWin::Hide();
    border_widget_->Hide();
  }

  void OnActivate(UINT action, BOOL minimized, HWND window) {
    NativeWidgetWin::OnActivate(action, minimized, window);
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
    NativeWidgetWin::OnSetFocus(focused_window);
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

void BrowserBubble::InitPopup(const gfx::Insets& content_margins) {
  // popup_ is a Widget, but we need to do some NativeWidgetWin stuff first,
  // then we'll assign it into popup_.
  BubbleWidget* bubble_widget = new BubbleWidget(this);

  BorderWidgetWin* border_widget = bubble_widget->border_widget();
  border_widget->InitBorderWidgetWin(new BorderContents,
                                     frame_->GetNativeView());
  border_widget->border_contents()->set_content_margins(content_margins);

  popup_ = bubble_widget->GetWidget();
  // We make the BorderWidgetWin the owner of the Bubble HWND, so that the
  // latter is displayed on top of the former.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.native_widget = bubble_widget;
  params.parent = border_widget->GetNativeView();
  popup_->Init(params);
  popup_->SetContentsView(view_);

  ResizeToView();
  Reposition();
  AttachToBrowser();
}

void BrowserBubble::Show(bool activate) {
  if (!popup_->IsVisible()) {
    static_cast<BubbleWidget*>(popup_->native_widget())->ShowAndActivate(
        activate);
  }
}

void BrowserBubble::Hide() {
  if (popup_->IsVisible())
    static_cast<BubbleWidget*>(popup_->native_widget())->Hide();
}

void BrowserBubble::ResizeToView() {
  BorderWidgetWin* border_widget =
      static_cast<BubbleWidget*>(popup_->native_widget())->border_widget();

  gfx::Rect window_bounds;
  window_bounds = border_widget->SizeAndGetBounds(GetAbsoluteRelativeTo(),
      arrow_location_, view_->size());

  SetAbsoluteBounds(window_bounds);
}
