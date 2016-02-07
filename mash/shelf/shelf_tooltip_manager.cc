// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/shelf/shelf_tooltip_manager.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "mash/shelf/shelf_view.h"
#include "mash/wm/public/interfaces/container.mojom.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace mash {
namespace shelf {
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
                     const base::string16& text,
                     ShelfTooltipManager* host);

  void SetText(const base::string16& text);
  void Close();

 private:
  // views::BubbleDelegateView overrides:
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;

  // views::WidgetDelegate overrides:
  bool ShouldShowWindowTitle() const override;
  void WindowClosing() override;

  // views::View overrides:
  gfx::Size GetPreferredSize() const override;

  ShelfTooltipManager* host_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(ShelfTooltipBubble);
};

ShelfTooltipManager::ShelfTooltipBubble::ShelfTooltipBubble(
    views::View* anchor,
    views::BubbleBorder::Arrow arrow,
    const base::string16& text,
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
  label_ = new views::Label(text);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetEnabledColor(kTooltipTextColor);
  AddChildView(label_);
  views::BubbleDelegateView::CreateBubble(this);
}

void ShelfTooltipManager::ShelfTooltipBubble::Close() {
  if (GetWidget()) {
    host_ = nullptr;
    GetWidget()->Close();
  }
}

void ShelfTooltipManager::ShelfTooltipBubble::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  // Ensure the widget is treated as a tooltip by the window manager.
  std::map<std::string, std::vector<uint8_t>> properties;
  properties[mash::wm::mojom::kWindowContainer_Property] =
      mojo::TypeConverter<const std::vector<uint8_t>, int32_t>::Convert(
          static_cast<int32_t>(mash::wm::mojom::Container::TOOLTIPS));
  mus::Window* window =
      views::WindowManagerConnection::Get()->NewWindow(properties);
  params->native_widget = new views::NativeWidgetMus(
      widget, host_->shelf_view()->shell(), window,
      mus::mojom::SurfaceType::DEFAULT);
}

void ShelfTooltipManager::ShelfTooltipBubble::WindowClosing() {
  views::BubbleDelegateView::WindowClosing();
  if (host_)
    host_->OnBubbleClosed(this);
}

bool ShelfTooltipManager::ShelfTooltipBubble::ShouldShowWindowTitle() const {
  return false;
}

gfx::Size ShelfTooltipManager::ShelfTooltipBubble::GetPreferredSize() const {
  gfx::Size pref_size = views::BubbleDelegateView::GetPreferredSize();
  if (pref_size.height() < kTooltipMinHeight)
    pref_size.set_height(kTooltipMinHeight);
  if (pref_size.width() > kTooltipMaxWidth)
    pref_size.set_width(kTooltipMaxWidth);
  return pref_size;
}

ShelfTooltipManager::ShelfTooltipManager(ShelfView* shelf_view)
    : view_(nullptr),
      widget_(nullptr),
      shelf_view_(shelf_view),
      weak_factory_(this) {
  // Ensure mouse movements between the shelf and its buttons isn't an exit.
  shelf_view_->set_notify_enter_exit_on_child(true);
  shelf_view_->AddPreTargetHandler(this);
}

ShelfTooltipManager::~ShelfTooltipManager() {
  CancelHidingAnimation();
  Close();
  shelf_view_->RemovePreTargetHandler(this);
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

  if (!shelf_view_->GetWidget() || !shelf_view_->GetWidget()->IsVisible())
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

  if (!shelf_view_->GetWidget() || !shelf_view_->GetWidget()->IsVisible())
    return;

  CreateBubble(anchor, text);
  ShowInternal();
}

void ShelfTooltipManager::Close() {
  StopTimer();
  if (view_) {
    view_->Close();
    view_ = nullptr;
    widget_ = nullptr;
  }
}

void ShelfTooltipManager::OnBubbleClosed(views::BubbleDelegateView* view) {
  if (view == view_) {
    view_ = nullptr;
    widget_ = nullptr;
  }
}

void ShelfTooltipManager::ResetTimer() {
  if (timer_.get() && timer_->IsRunning()) {
    timer_->Reset();
    return;
  }

  // We don't start the timer if the shelf isn't visible.
  if (!shelf_view_->GetWidget() || !shelf_view_->GetWidget()->IsVisible())
    return;

  CreateTimer(kTooltipAppearanceDelay);
}

void ShelfTooltipManager::StopTimer() {
  timer_.reset();
}

bool ShelfTooltipManager::IsVisible() const {
  if (timer_.get() && timer_->IsRunning())
    return false;

  return widget_ && widget_->IsVisible();
}

views::View* ShelfTooltipManager::GetCurrentAnchorView() const {
  return view_ ? view_->GetAnchorView() : nullptr;
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

  // Close the tooltip when the mouse is pressed or exits the shelf view area.
  if (event->type() == ui::ET_MOUSE_PRESSED ||
      (event->type() == ui::ET_MOUSE_EXITED &&
       event->target() == shelf_view_)) {
    CloseSoon();
    return;
  }

  gfx::Point location = event->location();
  views::View* target = static_cast<views::View*>(event->target());
  views::View::ConvertPointToTarget(target, shelf_view_, &location);
  if (shelf_view_->ShouldHideTooltip(location)) {
    // Because this mouse event may arrive to |view_|, here we just schedule
    // the closing event rather than directly calling Close().
    CloseSoon();
  }
}

void ShelfTooltipManager::OnTouchEvent(ui::TouchEvent* event) {
  // TODO(msw): Fix this; touch events outside the shelf are not captured.
  views::View* view = static_cast<views::View*>(event->target());
  aura::Window* target = view->GetWidget()->GetNativeWindow();
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

/* TODO(msw): Restore functionality:
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
}*/

void ShelfTooltipManager::CancelHidingAnimation() {
  if (!widget_ || !widget_->GetNativeView())
    return;

  ::wm::SetWindowVisibilityAnimationTransition(widget_->GetNativeView(),
                                               ::wm::ANIMATE_NONE);
}

void ShelfTooltipManager::CloseSoon() {
  base::MessageLoop::current()->PostTask(
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
  views::BubbleBorder::Arrow arrow = shelf_view_->SelectValueForShelfAlignment(
      views::BubbleBorder::BOTTOM_CENTER, views::BubbleBorder::LEFT_CENTER,
      views::BubbleBorder::RIGHT_CENTER, views::BubbleBorder::TOP_CENTER);
  view_ = new ShelfTooltipBubble(anchor, arrow, text, this);
  widget_ = view_->GetWidget();

  gfx::NativeView native_view = widget_->GetNativeView();
  ::wm::SetWindowVisibilityAnimationType(
      native_view, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  ::wm::SetWindowVisibilityAnimationTransition(
      native_view, ::wm::ANIMATE_HIDE);
}

void ShelfTooltipManager::CreateTimer(int delay_in_ms) {
  base::OneShotTimer* new_timer = new base::OneShotTimer();
  new_timer->Start(FROM_HERE,
                   base::TimeDelta::FromMilliseconds(delay_in_ms),
                   this,
                   &ShelfTooltipManager::ShowInternal);
  timer_.reset(new_timer);
}

}  // namespace shelf
}  // namespace mash
