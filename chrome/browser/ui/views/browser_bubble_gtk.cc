// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/browser_bubble.h"

#include <vector>

#include "chrome/browser/ui/views/bubble/border_contents.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

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
        bubble_(bubble),
        border_contents_(new BorderContents) {
    border_contents_->Init();
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

  BorderContents* border_contents() {
    return border_contents_;
  }

 private:
  BrowserBubble* bubble_;
  BorderContents* border_contents_;  // Owned by root view of this widget.

  DISALLOW_COPY_AND_ASSIGN(BubbleWidget);
};

}  // namespace

void BrowserBubble::InitPopup() {
  // TODO(port)
  BubbleWidget* pop = new BubbleWidget(this);
  pop->MakeTransparent();
  pop->make_transient_to_parent();
  pop->Init(frame_->GetNativeView(), gfx::Rect());
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

  views::View* contents_view = new views::View;

  // We add |contents_view| to ourselves before the AddChildView() call below so
  // that when |contents| gets added, it will already have a widget, and thus
  // any NativeButtons it creates in ViewHierarchyChanged() will be functional
  // (e.g. calling SetChecked() on checkboxes is safe).
  pop->SetContentsView(contents_view);

  // Added border_contents before |view_| so it will paint under it.
  contents_view->AddChildView(pop->border_contents());
  contents_view->AddChildView(view_);

  popup_ = pop;

  ResizeToView();
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

void BrowserBubble::ResizeToView() {
  BorderContents* border_contents =
      static_cast<BubbleWidget*>(popup_)->border_contents();

  // Calculate and set the bounds for all windows and views.
  gfx::Rect window_bounds;
  gfx::Rect contents_bounds;
  border_contents->SizeAndGetBounds(GetAbsoluteRelativeTo(),
      arrow_location_, false, view_->size(),
      &contents_bounds, &window_bounds);

  border_contents->SetBoundsRect(gfx::Rect(gfx::Point(), window_bounds.size()));
  view_->SetBoundsRect(contents_bounds);

  SetAbsoluteBounds(window_bounds);
}
