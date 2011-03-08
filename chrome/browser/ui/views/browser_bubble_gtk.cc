// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/browser_bubble.h"

#include <vector>

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/wm_ipc.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#endif

using std::vector;

namespace {

class BubbleWidget : public views::WidgetGtk {
 public:
  explicit BubbleWidget(BrowserBubble* bubble)
      : views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW),
        bubble_(bubble) {
  }

  void ShowAndActivate(bool activate) {
    // TODO: honor activate.
    views::WidgetGtk::Show();
  }

  virtual void Close() {
    if (!bubble_)
      return;  // We have already been closed.
    if (IsActive()) {
      BrowserBubble::Delegate* delegate = bubble_->delegate();
      if (delegate)
        delegate->BubbleLostFocus(bubble_, false);
    }
    views::WidgetGtk::Close();
    bubble_ = NULL;
  }

  virtual void Hide() {
    if (IsActive()&& bubble_) {
      BrowserBubble::Delegate* delegate = bubble_->delegate();
      if (delegate)
        delegate->BubbleLostFocus(bubble_, false);
    }
    views::WidgetGtk::Hide();
  }

  virtual void IsActiveChanged() {
    if (IsActive() || !bubble_)
      return;
    BrowserBubble::Delegate* delegate = bubble_->delegate();
    if (!delegate) {
      bubble_->DetachFromBrowser();
      delete bubble_;
      return;
    }

    // TODO(jcampan): http://crbugs.com/29131 Check if the window we are losing
    //                focus to is a child window and pass that to the call
    //                below.
    delegate->BubbleLostFocus(bubble_, false);
  }

  virtual gboolean OnFocusIn(GtkWidget* widget, GdkEventFocus* event) {
    if (bubble_ && bubble_->delegate())
      bubble_->delegate()->BubbleGotFocus(bubble_);
    return views::WidgetGtk::OnFocusIn(widget, event);
  }

 private:
  BrowserBubble* bubble_;

  DISALLOW_COPY_AND_ASSIGN(BubbleWidget);
};

}  // namespace

void BrowserBubble::InitPopup() {
  // TODO(port)
  views::WidgetGtk* pop = new BubbleWidget(this);
  pop->SetOpacity(0xFF);
  pop->make_transient_to_parent();
  pop->Init(frame_->GetNativeView(), bounds_);
#if defined(OS_CHROMEOS)
  {
    vector<int> params;
    params.push_back(0);  // don't show while screen is locked
    chromeos::WmIpc::instance()->SetWindowType(
        pop->GetNativeView(),
        chromeos::WM_IPC_WINDOW_CHROME_INFO_BUBBLE,
        &params);
  }
#endif
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
  static_cast<BubbleWidget*>(popup_)->ShowAndActivate(activate);
  visible_ = true;
}

void BrowserBubble::Hide() {
  if (!visible_)
    return;
  views::WidgetGtk* pop = static_cast<views::WidgetGtk*>(popup_);
  pop->Hide();
  visible_ = false;
}
