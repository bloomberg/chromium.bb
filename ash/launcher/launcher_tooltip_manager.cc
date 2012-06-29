// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_tooltip_manager.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_animations.h"
#include "base/time.h"
#include "base/timer.h"
#include "ui/aura/window.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {
namespace {
const int kTooltipMargin = 3;
const int kTooltipAppearanceDelay = 200;  // msec

// The maximum width of the tooltip bubble.  Borrowed the value from
// ash/tooltip/tooltip_controller.cc
const int kTooltipMaxWidth = 400;

views::BubbleBorder::ArrowLocation GetArrowLocation(ShelfAlignment alignment) {
  switch (alignment) {
    case SHELF_ALIGNMENT_LEFT:
      return views::BubbleBorder::LEFT_BOTTOM;
    case SHELF_ALIGNMENT_RIGHT:
      return views::BubbleBorder::RIGHT_BOTTOM;
    case SHELF_ALIGNMENT_BOTTOM:
      return views::BubbleBorder::BOTTOM_RIGHT;
  }

  return views::BubbleBorder::NONE;
}
}  // namespace

// The implementation of tooltip of the launcher.
class LauncherTooltipManager::LauncherTooltipBubble
    : public views::BubbleDelegateView {
 public:
  LauncherTooltipBubble(views::View* anchor,
                        views::BubbleBorder::ArrowLocation arrow_location,
                        LauncherTooltipManager* host);

  void SetText(const string16& text);

 private:
  // views::View overrides:
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;

  // views::WidgetDelegate overrides;
  virtual void WindowClosing() OVERRIDE;

  LauncherTooltipManager* host_;
  views::Label* label_;
};

LauncherTooltipManager::LauncherTooltipBubble::LauncherTooltipBubble(
    views::View* anchor,
    views::BubbleBorder::ArrowLocation arrow_location,
    LauncherTooltipManager* host)
    : views::BubbleDelegateView(anchor, arrow_location),
      host_(host) {
  set_close_on_esc(false);
  set_close_on_deactivate(false);
  set_use_focusless(true);
  set_margin(kTooltipMargin);
  SetLayoutManager(new views::FillLayout());
  // The anchor may not have the widget in tests.
  if (anchor->GetWidget() && anchor->GetWidget()->GetNativeView()) {
    aura::RootWindow* root_window =
        anchor->GetWidget()->GetNativeView()->GetRootWindow();
    set_parent_window(ash::Shell::GetInstance()->GetContainer(
        root_window, ash::internal::kShellWindowId_SettingBubbleContainer));
  }
  label_ = new views::Label;
  label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(label_);
}

void LauncherTooltipManager::LauncherTooltipBubble::SetText(
    const string16& text) {
  label_->SetText(text);
  label_->SetMultiLine(true);
  label_->SizeToFit(kTooltipMaxWidth);
  SizeToContents();
}

void LauncherTooltipManager::LauncherTooltipBubble::OnMouseExited(
    const views::MouseEvent& event) {
  GetWidget()->Close();
  host_->OnBubbleClosed(this);
}

void LauncherTooltipManager::LauncherTooltipBubble::WindowClosing() {
  views::BubbleDelegateView::WindowClosing();
  host_->OnBubbleClosed(this);
}

LauncherTooltipManager::LauncherTooltipManager(ShelfAlignment alignment)
    : view_(NULL),
      anchor_(NULL),
      alignment_(alignment) {}

LauncherTooltipManager::~LauncherTooltipManager() {
  Close();
}

void LauncherTooltipManager::ShowDelayed(views::View* anchor,
                                         const string16& text) {
  if (view_) {
    if (timer_.get() && timer_->IsRunning())
      return;
    else
      Close();
  }

  CreateBubble(anchor, text);
  gfx::NativeView native_view = view_->GetWidget()->GetNativeView();
  SetWindowVisibilityAnimationType(
      native_view, WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  SetWindowVisibilityAnimationTransition(native_view, ANIMATE_SHOW);
  ResetTimer();
}

void LauncherTooltipManager::ShowImmediately(views::View* anchor,
                                             const string16& text) {
  if (view_ && IsVisible())
      Close();

  CreateBubble(anchor, text);
  gfx::NativeView native_view = view_->GetWidget()->GetNativeView();
  SetWindowVisibilityAnimationTransition(native_view, ANIMATE_NONE);
  ShowInternal();
}

void LauncherTooltipManager::Close() {
  if (view_) {
    view_->GetWidget()->Close();
    view_ = NULL;
  }
}

void LauncherTooltipManager::OnBubbleClosed(
    views::BubbleDelegateView* view) {
  if (view == view_)
    view_ = NULL;
}

void LauncherTooltipManager::SetArrowLocation(ShelfAlignment alignment) {
  if (alignment_ == alignment)
    return;

  alignment_ = alignment;
  if (view_) {
    Close();
    ShowImmediately(anchor_, text_);
  }
}

void LauncherTooltipManager::ResetTimer() {
  if (timer_.get() && timer_->IsRunning()) {
    timer_->Reset();
    return;
  }

  base::OneShotTimer<LauncherTooltipManager>* new_timer =
      new base::OneShotTimer<LauncherTooltipManager>();
  new_timer->Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kTooltipAppearanceDelay),
      this,
      &LauncherTooltipManager::ShowInternal);
  timer_.reset(new_timer);
}

void LauncherTooltipManager::StopTimer() {
  timer_.reset();
}

bool LauncherTooltipManager::IsVisible() {
  if (timer_.get() && timer_->IsRunning())
    return false;

  return view_ && view_->GetWidget() && view_->GetWidget()->IsVisible();
}

void LauncherTooltipManager::ShowInternal() {
  if (view_)
    view_->Show();

  timer_.reset();
}

void LauncherTooltipManager::CreateBubble(views::View* anchor,
                                          const string16& text) {
  DCHECK(!view_);

  anchor_ = anchor;
  text_ = text;
  view_ = new LauncherTooltipBubble(
      anchor, GetArrowLocation(alignment_), this);
  views::BubbleDelegateView::CreateBubble(view_);
  view_->SetText(text_);
}

}  // namespace internal
}  // namespace ash
