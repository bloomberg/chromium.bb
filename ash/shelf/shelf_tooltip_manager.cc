// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_tooltip_manager.h"

#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_animations.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/insets.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {
const int kTooltipTopBottomMargin = 3;
const int kTooltipLeftRightMargin = 10;
const int kTooltipAppearanceDelay = 1000;  // msec
const int kTooltipMinHeight = 29 - 2 * kTooltipTopBottomMargin;
const SkColor kTooltipTextColor = SkColorSetRGB(0x22, 0x22, 0x22);

// The maximum width of the tooltip bubble.  Borrowed the value from
// ash/tooltip/tooltip_controller.cc
const int kTooltipMaxWidth = 250;

// The offset for the tooltip bubble - making sure that the bubble is flush
// with the shelf. The offset includes the arrow size in pixels as well as
// the activation bar and other spacing elements.
const int kArrowOffsetLeftRight = 11;
const int kArrowOffsetTopBottom = 7;

}  // namespace

// The implementation of tooltip of the launcher.
class ShelfTooltipManager::ShelfTooltipBubble
    : public views::BubbleDelegateView {
 public:
  ShelfTooltipBubble(views::View* anchor,
                        views::BubbleBorder::Arrow arrow,
                        ShelfTooltipManager* host);

  void SetText(const base::string16& text);
  void Close();

 private:
  // views::WidgetDelegate overrides:
  virtual void WindowClosing() OVERRIDE;

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

  ShelfTooltipManager* host_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(ShelfTooltipBubble);
};

ShelfTooltipManager::ShelfTooltipBubble::ShelfTooltipBubble(
    views::View* anchor,
    views::BubbleBorder::Arrow arrow,
    ShelfTooltipManager* host)
    : views::BubbleDelegateView(anchor, arrow), host_(host) {
  gfx::Insets insets = gfx::Insets(kArrowOffsetTopBottom,
                                   kArrowOffsetLeftRight,
                                   kArrowOffsetTopBottom,
                                   kArrowOffsetLeftRight);
  // Shelf items can have an asymmetrical border for spacing reasons.
  // Adjust anchor location for this.
  if (anchor->border())
    insets += anchor->border()->GetInsets();

  set_anchor_view_insets(insets);
  set_close_on_esc(false);
  set_close_on_deactivate(false);
  set_can_activate(false);
  set_accept_events(false);
  set_margins(gfx::Insets(kTooltipTopBottomMargin, kTooltipLeftRightMargin,
                          kTooltipTopBottomMargin, kTooltipLeftRightMargin));
  set_shadow(views::BubbleBorder::SMALL_SHADOW);
  SetLayoutManager(new views::FillLayout());
  // The anchor may not have the widget in tests.
  if (anchor->GetWidget() && anchor->GetWidget()->GetNativeView()) {
    aura::Window* root_window =
        anchor->GetWidget()->GetNativeView()->GetRootWindow();
    set_parent_window(ash::Shell::GetInstance()->GetContainer(
        root_window, ash::kShellWindowId_SettingBubbleContainer));
  }
  label_ = new views::Label;
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetEnabledColor(kTooltipTextColor);
  AddChildView(label_);
  views::BubbleDelegateView::CreateBubble(this);
}

void ShelfTooltipManager::ShelfTooltipBubble::SetText(
    const base::string16& text) {
  label_->SetText(text);
  SizeToContents();
}

void ShelfTooltipManager::ShelfTooltipBubble::Close() {
  if (GetWidget()) {
    host_ = NULL;
    GetWidget()->Close();
  }
}

void ShelfTooltipManager::ShelfTooltipBubble::WindowClosing() {
  views::BubbleDelegateView::WindowClosing();
  if (host_)
    host_->OnBubbleClosed(this);
}

gfx::Size ShelfTooltipManager::ShelfTooltipBubble::GetPreferredSize() const {
  gfx::Size pref_size = views::BubbleDelegateView::GetPreferredSize();
  if (pref_size.height() < kTooltipMinHeight)
    pref_size.set_height(kTooltipMinHeight);
  if (pref_size.width() > kTooltipMaxWidth)
    pref_size.set_width(kTooltipMaxWidth);
  return pref_size;
}

ShelfTooltipManager::ShelfTooltipManager(
    ShelfLayoutManager* shelf_layout_manager,
    ShelfView* shelf_view)
    : view_(NULL),
      widget_(NULL),
      anchor_(NULL),
      shelf_layout_manager_(shelf_layout_manager),
      shelf_view_(shelf_view),
      weak_factory_(this) {
  if (shelf_layout_manager)
    shelf_layout_manager->AddObserver(this);
  if (Shell::HasInstance())
    Shell::GetInstance()->AddPreTargetHandler(this);
}

ShelfTooltipManager::~ShelfTooltipManager() {
  CancelHidingAnimation();
  Close();
  if (shelf_layout_manager_)
    shelf_layout_manager_->RemoveObserver(this);
  if (Shell::HasInstance())
    Shell::GetInstance()->RemovePreTargetHandler(this);
}

void ShelfTooltipManager::ShowDelayed(views::View* anchor,
                                      const base::string16& text) {
  if (view_) {
    if (timer_.get() && timer_->IsRunning()) {
      return;
    } else {
      CancelHidingAnimation();
      Close();
    }
  }

  if (shelf_layout_manager_ && !shelf_layout_manager_->IsVisible())
    return;

  CreateBubble(anchor, text);
  ResetTimer();
}

void ShelfTooltipManager::ShowImmediately(views::View* anchor,
                                          const base::string16& text) {
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

void ShelfTooltipManager::Close() {
  StopTimer();
  if (view_) {
    view_->Close();
    view_ = NULL;
    widget_ = NULL;
  }
}

void ShelfTooltipManager::OnBubbleClosed(views::BubbleDelegateView* view) {
  if (view == view_) {
    view_ = NULL;
    widget_ = NULL;
  }
}

void ShelfTooltipManager::UpdateArrow() {
  if (view_) {
    CancelHidingAnimation();
    Close();
    ShowImmediately(anchor_, text_);
  }
}

void ShelfTooltipManager::ResetTimer() {
  if (timer_.get() && timer_->IsRunning()) {
    timer_->Reset();
    return;
  }

  // We don't start the timer if the shelf isn't visible.
  if (shelf_layout_manager_ && !shelf_layout_manager_->IsVisible())
    return;

  CreateTimer(kTooltipAppearanceDelay);
}

void ShelfTooltipManager::StopTimer() {
  timer_.reset();
}

bool ShelfTooltipManager::IsVisible() {
  if (timer_.get() && timer_->IsRunning())
    return false;

  return widget_ && widget_->IsVisible();
}

void ShelfTooltipManager::CreateZeroDelayTimerForTest() {
  CreateTimer(0);
}

void ShelfTooltipManager::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK(event);
  DCHECK(event->target());
  if (!widget_ || !widget_->IsVisible())
    return;

  DCHECK(view_);
  DCHECK(shelf_view_);

  // Pressing the mouse button anywhere should close the tooltip.
  if (event->type() == ui::ET_MOUSE_PRESSED) {
    CloseSoon();
    return;
  }

  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (widget_->GetNativeWindow()->GetRootWindow() != target->GetRootWindow()) {
    CloseSoon();
    return;
  }

  gfx::Point location_in_shelf_view = event->location();
  aura::Window::ConvertPointToTarget(
      target, shelf_view_->GetWidget()->GetNativeWindow(),
      &location_in_shelf_view);

  if (shelf_view_->ShouldHideTooltip(location_in_shelf_view)) {
    // Because this mouse event may arrive to |view_|, here we just schedule
    // the closing event rather than directly calling Close().
    CloseSoon();
  }
}

void ShelfTooltipManager::OnTouchEvent(ui::TouchEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (widget_ && widget_->IsVisible() && widget_->GetNativeWindow() != target)
    Close();
}

void ShelfTooltipManager::OnGestureEvent(ui::GestureEvent* event) {
  if (widget_ && widget_->IsVisible()) {
    // Because this mouse event may arrive to |view_|, here we just schedule
    // the closing event rather than directly calling Close().
    CloseSoon();
  }
}

void ShelfTooltipManager::OnCancelMode(ui::CancelModeEvent* event) {
  Close();
}

void ShelfTooltipManager::WillDeleteShelf() {
  shelf_layout_manager_ = NULL;
}

void ShelfTooltipManager::WillChangeVisibilityState(
    ShelfVisibilityState new_state) {
  if (new_state == SHELF_HIDDEN) {
    StopTimer();
    Close();
  }
}

void ShelfTooltipManager::OnAutoHideStateChanged(ShelfAutoHideState new_state) {
  if (new_state == SHELF_AUTO_HIDE_HIDDEN) {
    StopTimer();
    // AutoHide state change happens during an event filter, so immediate close
    // may cause a crash in the HandleMouseEvent() after the filter.  So we just
    // schedule the Close here.
    CloseSoon();
  }
}

void ShelfTooltipManager::CancelHidingAnimation() {
  if (!widget_ || !widget_->GetNativeView())
    return;

  gfx::NativeView native_view = widget_->GetNativeView();
  wm::SetWindowVisibilityAnimationTransition(
      native_view, wm::ANIMATE_NONE);
}

void ShelfTooltipManager::CloseSoon() {
  base::MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&ShelfTooltipManager::Close, weak_factory_.GetWeakPtr()));
}

void ShelfTooltipManager::ShowInternal() {
  if (view_)
    view_->GetWidget()->Show();

  timer_.reset();
}

void ShelfTooltipManager::CreateBubble(views::View* anchor,
                                       const base::string16& text) {
  DCHECK(!view_);

  anchor_ = anchor;
  text_ = text;
  views::BubbleBorder::Arrow arrow =
      shelf_layout_manager_->SelectValueForShelfAlignment(
          views::BubbleBorder::BOTTOM_CENTER,
          views::BubbleBorder::LEFT_CENTER,
          views::BubbleBorder::RIGHT_CENTER,
          views::BubbleBorder::TOP_CENTER);

  view_ = new ShelfTooltipBubble(anchor, arrow, this);
  widget_ = view_->GetWidget();
  view_->SetText(text_);

  gfx::NativeView native_view = widget_->GetNativeView();
  wm::SetWindowVisibilityAnimationType(
      native_view, wm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  wm::SetWindowVisibilityAnimationTransition(
      native_view, wm::ANIMATE_HIDE);
}

void ShelfTooltipManager::CreateTimer(int delay_in_ms) {
  base::OneShotTimer<ShelfTooltipManager>* new_timer =
      new base::OneShotTimer<ShelfTooltipManager>();
  new_timer->Start(FROM_HERE,
                   base::TimeDelta::FromMilliseconds(delay_in_ms),
                   this,
                   &ShelfTooltipManager::ShowInternal);
  timer_.reset(new_timer);
}

}  // namespace ash
