// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/browser_bubble.h"

#include <vector>

#include "chrome/browser/ui/views/bubble/border_contents.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/widget/native_widget_gtk.h"
#include "ui/views/widget/root_view.h"

#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/legacy_window_manager/wm_ipc.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"
#endif

using std::vector;

namespace {

class BubbleWidget : public views::NativeWidgetGtk {
 public:
  BubbleWidget(BrowserBubble* bubble, const gfx::Insets& content_margins)
      : views::NativeWidgetGtk(new views::Widget),
        bubble_(bubble),
        border_contents_(new BorderContents) {
    border_contents_->Init();
    border_contents_->set_content_margins(content_margins);
  }

  void ShowAndActivate(bool activate) {
    // TODO: honor activate.
    views::NativeWidgetGtk::Show();
  }

  virtual void Close() OVERRIDE {
    if (!bubble_)
      return;  // We have already been closed.
    if (IsActive()) {
      BrowserBubble::Delegate* delegate = bubble_->delegate();
      if (delegate)
        delegate->BubbleLostFocus(bubble_, false);
    }
    views::NativeWidgetGtk::Close();
    bubble_ = NULL;
  }

  virtual void Hide() OVERRIDE {
    if (IsActive() && bubble_) {
      BrowserBubble::Delegate* delegate = bubble_->delegate();
      if (delegate)
        delegate->BubbleLostFocus(bubble_, false);
    }
    views::NativeWidgetGtk::Hide();
  }

  virtual void OnActiveChanged() OVERRIDE {
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

  virtual gboolean OnFocusIn(GtkWidget* widget, GdkEventFocus* event) OVERRIDE {
    if (bubble_ && bubble_->delegate())
      bubble_->delegate()->BubbleGotFocus(bubble_);
    return views::NativeWidgetGtk::OnFocusIn(widget, event);
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

void BrowserBubble::InitPopup(const gfx::Insets& content_margins) {
  // TODO(port)
  BubbleWidget* bubble_widget = new BubbleWidget(this, content_margins);
  popup_ = bubble_widget->GetWidget();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.transparent = true;
  params.parent = frame_->GetNativeView();
  params.native_widget = bubble_widget;
  popup_->Init(params);
#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
  {
    vector<int> params;
    params.push_back(0);  // don't show while screen is locked
    chromeos::WmIpc::instance()->SetWindowType(
        popup_->GetNativeView(),
        chromeos::WM_IPC_WINDOW_CHROME_INFO_BUBBLE,
        &params);
  }
#endif

  views::View* contents_view = new views::View;

  // We add |contents_view| to ourselves before the AddChildView() call below so
  // that when |contents| gets added, it will already have a widget, and thus
  // any NativeButtons it creates in ViewHierarchyChanged() will be functional
  // (e.g. calling SetChecked() on checkboxes is safe).
  popup_->SetContentsView(contents_view);

  // Added border_contents before |view_| so it will paint under it.
  contents_view->AddChildView(bubble_widget->border_contents());
  contents_view->AddChildView(view_);

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
  BorderContents* border_contents =
      static_cast<BubbleWidget*>(popup_->native_widget())->border_contents();

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
