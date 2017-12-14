// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_tooltip_manager.h"

#include "ash/public/cpp/config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/base/material_design/material_design_controller.h"
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

const int kTooltipAppearanceDelay = 1000;  // msec

// Tooltip layout constants.

// Shelf item tooltip height.
const int kTooltipHeight = 24;

// The maximum width of the tooltip bubble.  Borrowed the value from
// ash/tooltip/tooltip_controller.cc
const int kTooltipMaxWidth = 250;

// Shelf item tooltip internal text margins.
const int kTooltipTopBottomMargin = 4;
const int kTooltipLeftRightMargin = 8;

// The offset for the tooltip bubble - making sure that the bubble is spaced
// with a fixed gap. The gap is accounted for by the transparent arrow in the
// bubble and an additional 1px padding for the shelf item views.
const int kArrowTopBottomOffset = 1;
const int kArrowLeftRightOffset = 1;

// Tooltip's border interior thickness that defines its minimum height.
const int kBorderInteriorThickness = kTooltipHeight / 2;

}  // namespace

// The implementation of tooltip of the launcher.
class ShelfTooltipManager::ShelfTooltipBubble
    : public views::BubbleDialogDelegateView {
 public:
  ShelfTooltipBubble(views::View* anchor,
                     views::BubbleBorder::Arrow arrow,
                     const base::string16& text)
      : views::BubbleDialogDelegateView(anchor, arrow) {
    set_close_on_deactivate(false);
    set_can_activate(false);
    set_accept_events(false);
    set_margins(gfx::Insets(kTooltipTopBottomMargin, kTooltipLeftRightMargin));
    set_shadow(views::BubbleBorder::NO_ASSETS);
    SetLayoutManager(std::make_unique<views::FillLayout>());
    views::Label* label = new views::Label(text);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    ui::NativeTheme* theme = anchor->GetWidget()->GetNativeTheme();
    label->SetEnabledColor(
        theme->GetSystemColor(ui::NativeTheme::kColorId_TooltipText));
    SkColor background_color =
        theme->GetSystemColor(ui::NativeTheme::kColorId_TooltipBackground);
    set_color(background_color);
    label->SetBackgroundColor(background_color);
    // The background is not opaque, so we can't do subpixel rendering.
    label->SetSubpixelRenderingEnabled(false);
    AddChildView(label);

    gfx::Insets insets(kArrowTopBottomOffset, kArrowLeftRightOffset);
    // Adjust the anchor location for asymmetrical borders of shelf item.
    if (anchor->border())
      insets += anchor->border()->GetInsets();
    if (ui::MaterialDesignController::IsSecondaryUiMaterial())
      insets += gfx::Insets(-kBubblePaddingHorizontalBottom);
    set_anchor_view_insets(insets);

    // Place the bubble in the same display as the anchor.
    set_parent_window(
        anchor_widget()->GetNativeWindow()->GetRootWindow()->GetChildById(
            kShellWindowId_SettingBubbleContainer));

    views::BubbleDialogDelegateView::CreateBubble(this);
    if (!ui::MaterialDesignController::IsSecondaryUiMaterial()) {
      // These must both be called after CreateBubble.
      SetArrowPaintType(views::BubbleBorder::PAINT_TRANSPARENT);
      SetBorderInteriorThickness(kBorderInteriorThickness);
    }
  }

 private:
  // BubbleDialogDelegateView overrides:
  gfx::Size CalculatePreferredSize() const override {
    const gfx::Size size = BubbleDialogDelegateView::CalculatePreferredSize();
    const int kTooltipMinHeight = kTooltipHeight - 2 * kTooltipTopBottomMargin;
    return gfx::Size(std::min(size.width(), kTooltipMaxWidth),
                     std::max(size.height(), kTooltipMinHeight));
  }

  int GetDialogButtons() const override { return ui::DIALOG_BUTTON_NONE; }

  DISALLOW_COPY_AND_ASSIGN(ShelfTooltipBubble);
};

ShelfTooltipManager::ShelfTooltipManager(ShelfView* shelf_view)
    : timer_delay_(kTooltipAppearanceDelay),
      shelf_view_(shelf_view),
      bubble_(nullptr),
      weak_factory_(this) {
  shelf_view_->shelf()->AddObserver(this);
  ShellPort::Get()->AddPointerWatcher(this,
                                      views::PointerWatcherEventTypes::BASIC);
}

ShelfTooltipManager::~ShelfTooltipManager() {
  ShellPort::Get()->RemovePointerWatcher(this);
  shelf_view_->shelf()->RemoveObserver(this);
  aura::Window* window = nullptr;
  if (shelf_view_->GetWidget())
    window = shelf_view_->GetWidget()->GetNativeWindow();
  if (window)
    wm::RemoveLimitedPreTargetHandlerForWindow(this, window);
}

void ShelfTooltipManager::Init() {
  wm::AddLimitedPreTargetHandlerForWindow(
      this, shelf_view_->GetWidget()->GetNativeWindow());
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
    ::wm::SetWindowVisibilityAnimationTransition(
        bubble_->GetWidget()->GetNativeWindow(), ::wm::ANIMATE_NONE);
    Close();
  }

  if (!ShouldShowTooltipForView(view))
    return;

  views::BubbleBorder::Arrow arrow = views::BubbleBorder::Arrow::NONE;
  switch (shelf_view_->shelf()->alignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      arrow = views::BubbleBorder::BOTTOM_CENTER;
      break;
    case SHELF_ALIGNMENT_LEFT:
      arrow = views::BubbleBorder::LEFT_CENTER;
      break;
    case SHELF_ALIGNMENT_RIGHT:
      arrow = views::BubbleBorder::RIGHT_CENTER;
      break;
  }

  base::string16 text = shelf_view_->GetTitleForView(view);
  bubble_ = new ShelfTooltipBubble(view, arrow, text);
  aura::Window* window = bubble_->GetWidget()->GetNativeWindow();
  ::wm::SetWindowVisibilityAnimationType(
      window, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  ::wm::SetWindowVisibilityAnimationTransition(window, ::wm::ANIMATE_HIDE);
  bubble_->GetWidget()->Show();
}

void ShelfTooltipManager::ShowTooltipWithDelay(views::View* view) {
  if (ShouldShowTooltipForView(view)) {
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(timer_delay_),
                 base::Bind(&ShelfTooltipManager::ShowTooltip,
                            weak_factory_.GetWeakPtr(), view));
  }
}

void ShelfTooltipManager::OnPointerEventObserved(
    const ui::PointerEvent& event,
    const gfx::Point& location_in_screen,
    gfx::NativeView target) {
  // Close on any press events inside or outside the tooltip.
  if (event.type() == ui::ET_POINTER_DOWN)
    Close();
}

void ShelfTooltipManager::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_EXITED) {
    Close();
    return;
  }

  if (event->type() != ui::ET_MOUSE_MOVED)
    return;

  // A workaround for crbug.com/756163, likely not needed as Mus/Mash matures.
  if (Shell::GetAshConfig() != Config::CLASSIC && event->location().IsOrigin())
    return;

  gfx::Point point = event->location();
  views::View::ConvertPointFromWidget(shelf_view_, &point);
  views::View* view = shelf_view_->GetTooltipHandlerForPoint(point);
  const bool should_show = ShouldShowTooltipForView(view);

  timer_.Stop();
  if (IsVisible() && should_show && bubble_->GetAnchorView() != view)
    ShowTooltip(view);
  else if (!IsVisible() && should_show)
    ShowTooltipWithDelay(view);
  else if (IsVisible() && shelf_view_->ShouldHideTooltip(point))
    Close();
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

bool ShelfTooltipManager::ShouldShowTooltipForView(views::View* view) {
  Shelf* shelf = shelf_view_ ? shelf_view_->shelf() : nullptr;
  return shelf && shelf_view_->visible() &&
         shelf_view_->ShouldShowTooltipForView(view) &&
         (shelf->GetVisibilityState() == SHELF_VISIBLE ||
          (shelf->GetVisibilityState() == SHELF_AUTO_HIDE &&
           shelf->GetAutoHideState() == SHELF_AUTO_HIDE_SHOWN));
}

}  // namespace ash
