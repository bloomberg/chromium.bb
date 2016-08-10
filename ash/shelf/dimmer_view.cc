// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/dimmer_view.h"

#include "ash/aura/wm_window_aura.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/shell.h"
#include "grit/ash_resources.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// Alpha to paint dimming image with.
const int kDimAlpha = 128;

// The time to dim and un-dim.
const int kTimeToDimMs = 3000;   // Slow in dimming.
const int kTimeToUnDimMs = 200;  // Fast in activating.

}  // namespace

// static
DimmerView* DimmerView::Create(WmShelf* shelf,
                               bool disable_animations_for_test) {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.accept_events = false;
  params.parent = WmWindowAura::GetAuraWindow(shelf->GetWindow());
  params.name = "DimmerView";
  views::Widget* dimmer = new views::Widget();
  dimmer->Init(params);
  dimmer->SetBounds(shelf->GetWindow()->GetBoundsInScreen());
  // The shelf should not take focus when it is initially shown.
  dimmer->set_focus_on_creation(false);
  DimmerView* view = new DimmerView(shelf, disable_animations_for_test);
  dimmer->SetContentsView(view);
  dimmer->Show();
  return view;
}

void DimmerView::SetHovered(bool hovered) {
  // Remember the hovered state so that we can correct the state once a
  // possible force state has disappeared.
  is_hovered_ = hovered;
  // Undim also if we were forced to by e.g. an open menu.
  hovered |= force_hovered_;
  background_animator_.SetDuration(hovered ? kTimeToUnDimMs : kTimeToDimMs);
  background_animator_.SetPaintsBackground(
      !hovered, disable_animations_for_test_ ? BACKGROUND_CHANGE_IMMEDIATE
                                             : BACKGROUND_CHANGE_ANIMATE);
}

void DimmerView::OnPaintBackground(gfx::Canvas* canvas) {
  SkPaint paint;
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia shelf_background =
      *rb->GetImageNamed(IDR_ASH_SHELF_DIMMING).ToImageSkia();

  if (!IsHorizontalAlignment(shelf_->GetAlignment())) {
    shelf_background = gfx::ImageSkiaOperations::CreateRotatedImage(
        shelf_background, shelf_->GetAlignment() == SHELF_ALIGNMENT_LEFT
                              ? SkBitmapOperations::ROTATION_90_CW
                              : SkBitmapOperations::ROTATION_270_CW);
  }
  paint.setAlpha(alpha_);
  canvas->DrawImageInt(shelf_background, 0, 0, shelf_background.width(),
                       shelf_background.height(), 0, 0, width(), height(),
                       false, paint);
}

views::Widget* DimmerView::GetWidget() {
  return View::GetWidget();
}

const views::Widget* DimmerView::GetWidget() const {
  return View::GetWidget();
}

void DimmerView::UpdateBackground(BackgroundAnimator* animator, int alpha) {
  alpha_ = alpha;
  SchedulePaint();
}

void DimmerView::BackgroundAnimationEnded(BackgroundAnimator* animator) {}

views::Widget* DimmerView::GetDimmerWidget() {
  return GetWidget();
}

void DimmerView::ForceUndimming(bool force) {
  bool previous = force_hovered_;
  force_hovered_ = force;
  // If the forced change does change the result we apply the change.
  if (is_hovered_ || force_hovered_ != is_hovered_ || previous)
    SetHovered(is_hovered_);
}

int DimmerView::GetDimmingAlphaForTest() {
  return alpha_;
}

void DimmerView::OnWindowBoundsChanged(WmWindow* window,
                                       const gfx::Rect& old_bounds,
                                       const gfx::Rect& new_bounds) {
  // Coming here the shelf got repositioned and since the dimmer is placed
  // in screen coordinates and not relative to the parent it needs to be
  // repositioned accordingly.
  GetWidget()->SetBounds(shelf_->GetWindow()->GetBoundsInScreen());
}

DimmerView::DimmerView(WmShelf* shelf, bool disable_animations_for_test)
    : shelf_(shelf),
      alpha_(kDimAlpha),
      is_hovered_(false),
      force_hovered_(false),
      disable_animations_for_test_(disable_animations_for_test),
      background_animator_(this, 0, kDimAlpha) {
  event_filter_.reset(new DimmerEventFilter(this));
  // Make sure it is undimmed at the beginning and then fire off the dimming
  // animation.
  background_animator_.SetPaintsBackground(false, BACKGROUND_CHANGE_IMMEDIATE);
  SetHovered(false);
  shelf_->GetWindow()->AddObserver(this);
}

DimmerView::~DimmerView() {
  // Some unit tests will come here with a destroyed window.
  if (shelf_->GetWindow())
    shelf_->GetWindow()->RemoveObserver(this);
}

DimmerView::DimmerEventFilter::DimmerEventFilter(DimmerView* owner)
    : owner_(owner), mouse_inside_(false), touch_inside_(false) {
  Shell::GetInstance()->AddPreTargetHandler(this);
}

DimmerView::DimmerEventFilter::~DimmerEventFilter() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void DimmerView::DimmerEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSE_MOVED &&
      event->type() != ui::ET_MOUSE_DRAGGED)
    return;

  gfx::Point screen_point(event->location());
  ::wm::ConvertPointToScreen(static_cast<aura::Window*>(event->target()),
                             &screen_point);
  bool inside = owner_->GetBoundsInScreen().Contains(screen_point);
  if (mouse_inside_ || touch_inside_ != inside || touch_inside_)
    owner_->SetHovered(inside || touch_inside_);
  mouse_inside_ = inside;
}

void DimmerView::DimmerEventFilter::OnTouchEvent(ui::TouchEvent* event) {
  bool touch_inside = false;
  if (event->type() != ui::ET_TOUCH_RELEASED &&
      event->type() != ui::ET_TOUCH_CANCELLED) {
    gfx::Point screen_point(event->location());
    ::wm::ConvertPointToScreen(static_cast<aura::Window*>(event->target()),
                               &screen_point);
    touch_inside = owner_->GetBoundsInScreen().Contains(screen_point);
  }

  if (mouse_inside_ || touch_inside_ != mouse_inside_ || touch_inside)
    owner_->SetHovered(mouse_inside_ || touch_inside);
  touch_inside_ = touch_inside;
}

}  // namespace ash
