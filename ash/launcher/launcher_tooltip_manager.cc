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
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_constants.h"
#include "ui/gfx/insets.h"
#include "ui/views/bubble/bubble_border_2.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {
namespace {
const int kTooltipTopBottomMargin = 3;
const int kTooltipLeftRightMargin = 10;
const int kTooltipAppearanceDelay = 200;  // msec
const int kTooltipMinHeight = 29 - 2 * kTooltipTopBottomMargin;
const SkColor kTooltipTextColor = SkColorSetRGB(0x22, 0x22, 0x22);

// The maximum width of the tooltip bubble.  Borrowed the value from
// ash/tooltip/tooltip_controller.cc
const int kTooltipMaxWidth = 250;

// Bubble border metrics
const int kArrowHeight = 7;
const int kArrowWidth = 15;
const int kShadowWidth = 8;
// The distance between the arrow tip and edge of the anchor view.
const int kArrowOffset = 10;

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
  void Close();

 private:
  // Overridden from views::BubbleDelegateView:
  virtual gfx::Rect GetBubbleBounds() OVERRIDE;

  // views::WidgetDelegate overrides:
  virtual void WindowClosing() OVERRIDE;

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  LauncherTooltipManager* host_;
  views::Label* label_;
  views::BubbleBorder2* bubble_border_;

  DISALLOW_COPY_AND_ASSIGN(LauncherTooltipBubble);
};

LauncherTooltipManager::LauncherTooltipBubble::LauncherTooltipBubble(
    views::View* anchor,
    views::BubbleBorder::ArrowLocation arrow_location,
    LauncherTooltipManager* host)
    : views::BubbleDelegateView(anchor, arrow_location),
      host_(host),
      bubble_border_(NULL) {
  set_close_on_esc(false);
  set_close_on_deactivate(false);
  set_use_focusless(true);
  set_accept_events(false);
  set_margins(gfx::Insets(kTooltipTopBottomMargin, kTooltipLeftRightMargin,
                          kTooltipTopBottomMargin, kTooltipLeftRightMargin));
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
  label_->SetEnabledColor(kTooltipTextColor);
  label_->SetElideBehavior(views::Label::ELIDE_AT_END);
  AddChildView(label_);
  views::BubbleDelegateView::CreateBubble(this);
  bubble_border_ = new views::BubbleBorder2(views::BubbleBorder::BOTTOM_RIGHT);
  bubble_border_->SetShadow(gfx::ShadowValue(
      gfx::Point(0, 5), kShadowWidth, SkColorSetARGB(0x72, 0, 0, 0)));
  bubble_border_->set_arrow_width(kArrowWidth);
  bubble_border_->set_arrow_height(kArrowHeight);
  set_anchor_insets(gfx::Insets(kArrowOffset, kArrowOffset, kArrowOffset,
      kArrowOffset));
  GetBubbleFrameView()->SetBubbleBorder(bubble_border_);

  // BubbleBorder2 paints its own background.
  GetBubbleFrameView()->set_background(NULL);
}

void LauncherTooltipManager::LauncherTooltipBubble::SetText(
    const string16& text) {
  label_->SetText(text);
  SizeToContents();
}

void LauncherTooltipManager::LauncherTooltipBubble::Close() {
  if (GetWidget()) {
    host_ = NULL;
    GetWidget()->Close();
  }
}

gfx::Rect LauncherTooltipManager::LauncherTooltipBubble::GetBubbleBounds() {
  // This happens before replacing the default border.
  if (!bubble_border_)
    return views::BubbleDelegateView::GetBubbleBounds();

  const gfx::Rect anchor_rect = GetAnchorRect();
  gfx::Rect bubble_rect = GetBubbleFrameView()->GetUpdatedWindowBounds(
      anchor_rect,
      GetPreferredSize(),
      false /* try_mirroring_arrow */);

  const gfx::Point old_offset = bubble_border_->offset();
  bubble_rect = bubble_border_->ComputeOffsetAndUpdateBubbleRect(bubble_rect,
      anchor_rect);

  // Repaints border if arrow offset is changed.
  if (bubble_border_->offset() != old_offset)
    GetBubbleFrameView()->SchedulePaint();

  return bubble_rect;
}

void LauncherTooltipManager::LauncherTooltipBubble::WindowClosing() {
  views::BubbleDelegateView::WindowClosing();
  if (host_)
    host_->OnBubbleClosed(this);
}

gfx::Size LauncherTooltipManager::LauncherTooltipBubble::GetPreferredSize() {
  gfx::Size pref_size = views::BubbleDelegateView::GetPreferredSize();
  if (pref_size.height() < kTooltipMinHeight)
    pref_size.set_height(kTooltipMinHeight);
  if (pref_size.width() > kTooltipMaxWidth)
    pref_size.set_width(kTooltipMaxWidth);
  return pref_size;
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
  CancelHidingAnimation();
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
    else {
      CancelHidingAnimation();
      Close();
    }
  }

  if (shelf_layout_manager_ && !shelf_layout_manager_->IsVisible())
    return;

  CreateBubble(anchor, text);
  ResetTimer();
}

void LauncherTooltipManager::ShowImmediately(views::View* anchor,
                                             const string16& text) {
  if (view_) {
    if (timer_.get() && timer_->IsRunning())
      StopTimer();
    CancelHidingAnimation();
    Close();
  }

  if (shelf_layout_manager_ && !shelf_layout_manager_->IsVisible())
    return;

  CreateBubble(anchor, text);
  ShowInternal();
}

void LauncherTooltipManager::Close() {
  StopTimer();
  if (view_) {
    view_->Close();
    view_ = NULL;
    widget_ = NULL;
  }
}

void LauncherTooltipManager::OnBubbleClosed(views::BubbleDelegateView* view) {
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
    CancelHidingAnimation();
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
                                               ui::KeyEvent* event) {
  // Not handled.
  return false;
}

bool LauncherTooltipManager::PreHandleMouseEvent(aura::Window* target,
                                                 ui::MouseEvent* event) {
  DCHECK(target);
  DCHECK(event);
  if (!widget_ || !widget_->IsVisible())
    return false;

  DCHECK(view_);
  DCHECK(launcher_view_);

  if (widget_->GetNativeWindow()->GetRootWindow() != target->GetRootWindow()) {
    CloseSoon();
    return false;
  }

  gfx::Point location_in_launcher_view = event->location();
  aura::Window::ConvertPointToTarget(
      target, launcher_view_->GetWidget()->GetNativeWindow(),
      &location_in_launcher_view);

  gfx::Point location_on_screen = event->location();
  aura::Window::ConvertPointToTarget(
      target, target->GetRootWindow(), &location_on_screen);
  gfx::Rect bubble_rect = widget_->GetWindowBoundsInScreen();

  if (launcher_view_->ShouldHideTooltip(location_in_launcher_view) &&
      !bubble_rect.Contains(location_on_screen)) {
    // Because this mouse event may arrive to |view_|, here we just schedule
    // the closing event rather than directly calling Close().
    CloseSoon();
  }

  return false;
}

ui::TouchStatus LauncherTooltipManager::PreHandleTouchEvent(
    aura::Window* target, ui::TouchEvent* event) {
  if (widget_ && widget_->IsVisible() && widget_->GetNativeWindow() != target)
    Close();
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::EventResult LauncherTooltipManager::PreHandleGestureEvent(
    aura::Window* target, ui::GestureEvent* event) {
  if (widget_ && widget_->IsVisible()) {
    // Because this mouse event may arrive to |view_|, here we just schedule
    // the closing event rather than directly calling Close().
    CloseSoon();
  }

  return ui::ER_UNHANDLED;
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
    CloseSoon();
  }
}

void LauncherTooltipManager::CancelHidingAnimation() {
  if (!widget_ || !widget_->GetNativeView())
    return;

  gfx::NativeView native_view = widget_->GetNativeView();
  SetWindowVisibilityAnimationTransition(native_view, ANIMATE_NONE);
}

void LauncherTooltipManager::CloseSoon() {
  MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&LauncherTooltipManager::Close, base::Unretained(this)));
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
  widget_ = view_->GetWidget();
  view_->SetText(text_);

  gfx::NativeView native_view = widget_->GetNativeView();
  SetWindowVisibilityAnimationType(
      native_view, WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  SetWindowVisibilityAnimationTransition(native_view, ANIMATE_HIDE);
}

}  // namespace internal
}  // namespace ash
