// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_tooltip_manager.h"

#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_animations.h"
#include "base/bind.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

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
    : public views::BubbleDialogDelegateView {
 public:
  ShelfTooltipBubble(views::View* anchor,
                     views::BubbleBorder::Arrow arrow,
                     const base::string16& text)
      : views::BubbleDialogDelegateView(anchor, arrow) {
    gfx::Insets insets =
        gfx::Insets(kArrowOffsetTopBottom, kArrowOffsetLeftRight);
    // Adjust the anchor location for asymmetrical borders of shelf item.
    if (anchor->border())
      insets += anchor->border()->GetInsets();

    set_anchor_view_insets(insets);
    set_close_on_deactivate(false);
    set_can_activate(false);
    set_accept_events(false);
    set_margins(gfx::Insets(kTooltipTopBottomMargin, kTooltipLeftRightMargin));
    set_shadow(views::BubbleBorder::SMALL_SHADOW);
    SetLayoutManager(new views::FillLayout());
    // The anchor may not have the widget in tests.
    if (anchor->GetWidget() && anchor->GetWidget()->GetNativeWindow()) {
      set_parent_window(ash::Shell::GetContainer(
          anchor->GetWidget()->GetNativeWindow()->GetRootWindow(),
          ash::kShellWindowId_SettingBubbleContainer));
    }
    views::Label* label = new views::Label(text);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetEnabledColor(kTooltipTextColor);
    AddChildView(label);
    views::BubbleDialogDelegateView::CreateBubble(this);
  }

 private:
  // BubbleDialogDelegateView overrides:
  gfx::Size GetPreferredSize() const override {
    const gfx::Size size = BubbleDialogDelegateView::GetPreferredSize();
    return gfx::Size(std::min(size.width(), kTooltipMaxWidth),
                     std::max(size.height(), kTooltipMinHeight));
  }

  int GetDialogButtons() const override { return ui::DIALOG_BUTTON_NONE; }

  DISALLOW_COPY_AND_ASSIGN(ShelfTooltipBubble);
};

ShelfTooltipManager::ShelfTooltipManager(ShelfView* shelf_view)
    : timer_delay_(kTooltipAppearanceDelay),
      shelf_view_(shelf_view),
      root_window_(nullptr),
      shelf_layout_manager_(nullptr),
      bubble_(nullptr),
      weak_factory_(this) {}

ShelfTooltipManager::~ShelfTooltipManager() {
  WillDeleteShelf();
  if (root_window_) {
    root_window_->RemoveObserver(this);
    root_window_->RemovePreTargetHandler(this);
    root_window_ = nullptr;
  }
}

void ShelfTooltipManager::Init() {
  shelf_layout_manager_ = shelf_view_->shelf()->shelf_layout_manager();
  shelf_layout_manager_->AddObserver(this);

  // TODO(msw): Observe events outside the shelf in mash, to close tooltips.
  root_window_ = shelf_view_->GetWidget()->GetNativeWindow()->GetRootWindow();
  root_window_->AddPreTargetHandler(this);
  root_window_->AddObserver(this);
}

void ShelfTooltipManager::Close() {
  timer_.Stop();
  if (bubble_)
    bubble_->GetWidget()->Close();
  bubble_ = nullptr;
}

bool ShelfTooltipManager::IsVisible() const {
  return bubble_ && bubble_->GetWidget()->IsVisible();
}

views::View* ShelfTooltipManager::GetCurrentAnchorView() const {
  return bubble_ ? bubble_->GetAnchorView() : nullptr;
}

void ShelfTooltipManager::ShowTooltip(views::View* view) {
  timer_.Stop();
  if (bubble_) {
    // Cancel the hiding animation to hide the old bubble immediately.
    gfx::NativeView native_view = bubble_->GetWidget()->GetNativeView();
    ::wm::SetWindowVisibilityAnimationTransition(native_view,
                                                 ::wm::ANIMATE_NONE);
    Close();
  }

  if (shelf_layout_manager_ && !shelf_layout_manager_->IsVisible())
    return;

  Shelf* shelf = shelf_view_->shelf();
  views::BubbleBorder::Arrow arrow = shelf->SelectValueForShelfAlignment(
      views::BubbleBorder::BOTTOM_CENTER, views::BubbleBorder::LEFT_CENTER,
      views::BubbleBorder::RIGHT_CENTER);

  base::string16 text = shelf_view_->GetTitleForView(view);
  bubble_ = new ShelfTooltipBubble(view, arrow, text);
  gfx::NativeView native_view = bubble_->GetWidget()->GetNativeView();
  ::wm::SetWindowVisibilityAnimationType(
      native_view, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  ::wm::SetWindowVisibilityAnimationTransition(native_view, ::wm::ANIMATE_HIDE);
  bubble_->GetWidget()->Show();
}

void ShelfTooltipManager::ShowTooltipWithDelay(views::View* view) {
  if (!shelf_layout_manager_ || shelf_layout_manager_->IsVisible()) {
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(timer_delay_),
                 base::Bind(&ShelfTooltipManager::ShowTooltip,
                            weak_factory_.GetWeakPtr(), view));
  }
}

void ShelfTooltipManager::OnEvent(ui::Event* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED ||
      event->type() == ui::ET_MOUSE_EXITED || !event->IsMouseEvent() ||
      event->target() != shelf_view_->GetWidget()->GetNativeWindow()) {
    if (!event->IsKeyEvent())
      Close();
    return;
  }

  gfx::Point point = static_cast<ui::LocatedEvent*>(event)->location();
  aura::Window::ConvertPointToTarget(
      static_cast<aura::Window*>(event->target()),
      shelf_view_->GetWidget()->GetNativeWindow(), &point);
  views::View::ConvertPointFromWidget(shelf_view_, &point);
  views::View* view = shelf_view_->GetTooltipHandlerForPoint(point);
  const bool should_show = shelf_view_->ShouldShowTooltipForView(view);

  timer_.Stop();
  if (IsVisible() && should_show && bubble_->GetAnchorView() != view)
    ShowTooltip(view);
  else if (!IsVisible() && should_show && event->type() == ui::ET_MOUSE_MOVED)
    ShowTooltipWithDelay(view);
  else if (IsVisible() && shelf_view_->ShouldHideTooltip(point))
    Close();
}

void ShelfTooltipManager::OnWindowDestroying(aura::Window* window) {
  if (window == root_window_) {
    root_window_->RemoveObserver(this);
    root_window_->RemovePreTargetHandler(this);
    root_window_ = nullptr;
  }
}

void ShelfTooltipManager::WillDeleteShelf() {
  if (shelf_layout_manager_)
    shelf_layout_manager_->RemoveObserver(this);
  shelf_layout_manager_ = nullptr;
  shelf_view_ = nullptr;
}

void ShelfTooltipManager::WillChangeVisibilityState(
    ShelfVisibilityState new_state) {
  if (new_state == SHELF_HIDDEN)
    Close();
}

void ShelfTooltipManager::OnAutoHideStateChanged(ShelfAutoHideState new_state) {
  if (new_state == SHELF_AUTO_HIDE_HIDDEN) {
    timer_.Stop();
    // AutoHide state change happens during an event filter, so immediate close
    // may cause a crash in the HandleMouseEvent() after the filter.  So we just
    // schedule the Close here.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&ShelfTooltipManager::Close, weak_factory_.GetWeakPtr()));
  }
}

}  // namespace ash
