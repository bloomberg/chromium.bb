// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_tooltip_manager.h"

#include "ash/launcher/launcher_view.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_animations.h"
#include "base/bind.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/timer.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/events.h"
#include "ui/gfx/insets.h"
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
  set_margins(gfx::Insets(kTooltipMargin, kTooltipMargin, kTooltipMargin,
                          kTooltipMargin));
  SetLayoutManager(new views::FillLayout());
  // The anchor may not have the widget in tests.
  if (anchor->GetWidget() && anchor->GetWidget()->GetNativeView()) {
    aura::RootWindow* root_window =
        anchor->GetWidget()->GetNativeView()->GetRootWindow();
    set_parent_window(ash::Shell::GetInstance()->GetContainer(
        root_window, ash::internal::kShellWindowId_LauncherContainer));
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

void LauncherTooltipManager::LauncherTooltipBubble::WindowClosing() {
  views::BubbleDelegateView::WindowClosing();
  host_->OnBubbleClosed(this);
}

LauncherTooltipManager::LauncherTooltipManager(
    ShelfAlignment alignment,
    ShelfLayoutManager* shelf_layout_manager,
    LauncherView* launcher_view)
    : view_(NULL),
      widget_(NULL),
      anchor_(NULL),
      alignment_(alignment),
      shelf_layout_manager_(shelf_layout_manager),
      launcher_view_(launcher_view) {
  if (shelf_layout_manager)
    shelf_layout_manager->AddObserver(this);
  if (Shell::HasInstance())
    Shell::GetInstance()->AddEnvEventFilter(this);
}

LauncherTooltipManager::~LauncherTooltipManager() {
  Close();
  if (shelf_layout_manager_)
    shelf_layout_manager_->RemoveObserver(this);
  if (Shell::HasInstance())
    Shell::GetInstance()->RemoveEnvEventFilter(this);
}

void LauncherTooltipManager::ShowDelayed(views::View* anchor,
                                         const string16& text) {
  if (view_) {
    if (timer_.get() && timer_->IsRunning())
      return;
    else
      Close();
  }

  if (shelf_layout_manager_ && !shelf_layout_manager_->IsVisible())
    return;

  CreateBubble(anchor, text);
  gfx::NativeView native_view = widget_->GetNativeView();
  SetWindowVisibilityAnimationType(
      native_view, WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  SetWindowVisibilityAnimationTransition(native_view, ANIMATE_SHOW);
  ResetTimer();
}

void LauncherTooltipManager::ShowImmediately(views::View* anchor,
                                             const string16& text) {
  if (view_) {
    if (timer_.get() && timer_->IsRunning())
      StopTimer();
    Close();
  }

  if (shelf_layout_manager_ && !shelf_layout_manager_->IsVisible())
    return;

  CreateBubble(anchor, text);
  gfx::NativeView native_view = widget_->GetNativeView();
  SetWindowVisibilityAnimationTransition(native_view, ANIMATE_NONE);
  ShowInternal();
}

void LauncherTooltipManager::Close() {
  if (widget_) {
    if (widget_->IsVisible())
      widget_->Close();
    view_ = NULL;
    widget_ = NULL;
  }
}

void LauncherTooltipManager::OnBubbleClosed(
    views::BubbleDelegateView* view) {
  if (view == view_) {
    view_ = NULL;
    widget_ = NULL;
  }
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

  // We don't start the timer if the shelf isn't visible.
  if (shelf_layout_manager_ && !shelf_layout_manager_->IsVisible())
    return;

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

  return widget_ && widget_->IsVisible();
}

bool LauncherTooltipManager::PreHandleKeyEvent(aura::Window* target,
                                               aura::KeyEvent* event) {
  // Not handled.
  return false;
}

bool LauncherTooltipManager::PreHandleMouseEvent(aura::Window* target,
                                                 aura::MouseEvent* event) {
  DCHECK(target);
  DCHECK(event);
  if (!widget_ || !widget_->IsVisible())
    return false;

  DCHECK(view_);
  DCHECK(launcher_view_);

  if (widget_->GetNativeWindow()->GetRootWindow() != target->GetRootWindow()) {
    MessageLoopForUI::current()->PostTask(
        FROM_HERE,
        base::Bind(&LauncherTooltipManager::Close, base::Unretained(this)));
    return false;
  }

  gfx::Point location_in_launcher_view = event->location();
  aura::Window::ConvertPointToWindow(
      target, launcher_view_->GetWidget()->GetNativeWindow(),
      &location_in_launcher_view);

  gfx::Point location_on_screen = event->location();
  aura::Window::ConvertPointToWindow(
      target, target->GetRootWindow(), &location_on_screen);
  gfx::Rect bubble_rect = widget_->GetWindowBoundsInScreen();

  if (launcher_view_->ShouldHideTooltip(location_in_launcher_view) &&
      !bubble_rect.Contains(location_on_screen)) {
    // Because this mouse event may arrive to |view_|, here we just schedule
    // the closing event rather than directly calling Close().
    MessageLoopForUI::current()->PostTask(
        FROM_HERE,
        base::Bind(&LauncherTooltipManager::Close, base::Unretained(this)));
  }

  return false;
}

ui::TouchStatus LauncherTooltipManager::PreHandleTouchEvent(
    aura::Window* target, aura::TouchEvent* event) {
  if (widget_ && widget_->IsVisible() && widget_->GetNativeWindow() != target)
    Close();
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus LauncherTooltipManager::PreHandleGestureEvent(
    aura::Window* target, aura::GestureEvent* event) {
  if (widget_ && widget_->IsVisible()) {
    // Because this mouse event may arrive to |view_|, here we just schedule
    // the closing event rather than directly calling Close().
    MessageLoopForUI::current()->PostTask(
        FROM_HERE,
        base::Bind(&LauncherTooltipManager::Close, base::Unretained(this)));
  }

  return ui::GESTURE_STATUS_UNKNOWN;
}

void LauncherTooltipManager::WillDeleteShelf() {
  shelf_layout_manager_ = NULL;
}

void LauncherTooltipManager::WillChangeVisibilityState(
    ShelfLayoutManager::VisibilityState new_state) {
  if (new_state == ShelfLayoutManager::HIDDEN) {
    StopTimer();
    Close();
  }
}

void LauncherTooltipManager::OnAutoHideStateChanged(
    ShelfLayoutManager::AutoHideState new_state) {
  if (new_state == ShelfLayoutManager::AUTO_HIDE_HIDDEN) {
    StopTimer();
    // AutoHide state change happens during an event filter, so immediate close
    // may cause a crash in the HandleMouseEvent() after the filter.  So we just
    // schedule the Close here.
    MessageLoopForUI::current()->PostTask(
        FROM_HERE,
        base::Bind(&LauncherTooltipManager::Close, base::Unretained(this)));
  }
}

void LauncherTooltipManager::ShowInternal() {
  if (view_) {
    view_->Show();
  }

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
  widget_ = view_->GetWidget();
  view_->SetText(text_);
}

}  // namespace internal
}  // namespace ash
