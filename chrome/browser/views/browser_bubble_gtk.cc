// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/browser_bubble.h"

#include "chrome/browser/views/frame/browser_view.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window.h"

#include "chrome/browser/views/tabs/tab_overview_types.h"

namespace {

class BubbleWidget : public views::WidgetGtk {
 public:
  explicit BubbleWidget(BrowserBubble* bubble)
      : views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW),
        bubble_(bubble),
        closed_(false) {
  }

  void Show(bool activate) {
    // TODO: honor activate.
    views::WidgetGtk::Show();
  }

  virtual void Close() {
    if (closed_)
      return;
    closed_ = true;
    if (IsActive()) {
      BrowserBubble::Delegate* delegate = bubble_->delegate();
      if (delegate)
        delegate->BubbleLostFocus(bubble_, NULL);
    }
    views::WidgetGtk::Close();
  }

  virtual void Hide() {
    if (IsActive()) {
      BrowserBubble::Delegate* delegate = bubble_->delegate();
      if (delegate)
        delegate->BubbleLostFocus(bubble_, NULL);
    }
    views::WidgetGtk::Hide();
  }

  virtual void IsActiveChanged() {
    if (IsActive() || closed_)
      return;
    BrowserBubble::Delegate* delegate = bubble_->delegate();
    if (!delegate) {
      bubble_->DetachFromBrowser();
      delete bubble_;
      return;
    }

    delegate->BubbleLostFocus(bubble_, GetNativeView());
  }

  virtual gboolean OnFocusIn(GtkWidget* widget, GdkEventFocus* event) {
    BrowserBubble::Delegate* delegate = bubble_->delegate();
    if (delegate)
      delegate->BubbleGotFocus(bubble_);
    return views::WidgetGtk::OnFocusIn(widget, event);
  }

 private:
  BrowserBubble* bubble_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(BubbleWidget);
};

}  // namespace

void BrowserBubble::InitPopup() {
  views::WidgetGtk* pop = new BubbleWidget(this);
  pop->SetOpacity(0xFF);
  pop->make_transient_to_parent();
  pop->Init(frame_->GetNativeView(), bounds_);
  TabOverviewTypes::instance()->SetWindowType(
      pop->GetNativeView(),
      TabOverviewTypes::WINDOW_TYPE_CHROME_INFO_BUBBLE,
      NULL);
  pop->SetContentsView(view_);
  popup_ = pop;
  Reposition();
  AttachToBrowser();
}

void BrowserBubble::MovePopup(int x, int y, int w, int h) {
  views::WidgetGtk* pop = static_cast<views::WidgetGtk*>(popup_);
  pop->SetBounds(gfx::Rect(x, y, w, h));
}

void BrowserBubble::Show(bool activate) {
  if (visible_)
    return;
  static_cast<BubbleWidget*>(popup_)->Show(activate);
  visible_ = true;
}

void BrowserBubble::Hide() {
  if (!visible_)
    return;
  views::WidgetGtk* pop = static_cast<views::WidgetGtk*>(popup_);
  pop->Hide();
  visible_ = false;
}
