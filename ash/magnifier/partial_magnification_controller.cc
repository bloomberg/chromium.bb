// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/partial_magnification_controller.h"

#include "ash/common/system/chromeos/palette/palette_utils.h"
#include "ash/shell.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// Ratio of magnifier scale.
const float kMagnificationScale = 2.f;
// Radius of the magnifying glass in DIP.
const int kMagnifierRadius = 200;
// Size of the border around the magnifying glass in DIP.
const int kBorderSize = 10;
// Thickness of the outline around magnifying glass border in DIP.
const int kBorderOutlineThickness = 2;
// The color of the border and its outlines. The border has an outline on both
// sides, producing a black/white/black ring.
const SkColor kBorderColor = SK_ColorWHITE;
const SkColor kBorderOutlineColor = SK_ColorBLACK;
// Inset on the zoom filter.
const int kZoomInset = 0;
// Vertical offset between the center of the magnifier and the tip of the
// pointer. TODO(jdufault): The vertical offset should only apply to the window
// location, not the magnified contents. See crbug.com/637617.
const int kVerticalOffset = 0;

// Name of the magnifier window.
const char kPartialMagniferWindowName[] = "PartialMagnifierWindow";

gfx::Size GetWindowSize() {
  return gfx::Size(kMagnifierRadius * 2, kMagnifierRadius * 2);
}

gfx::Rect GetBounds(gfx::Point mouse) {
  gfx::Size size = GetWindowSize();
  gfx::Point origin(mouse.x() - (size.width() / 2),
                    mouse.y() - (size.height() / 2) - kVerticalOffset);
  return gfx::Rect(origin, size);
}

aura::Window* GetCurrentRootWindow() {
  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();
  for (aura::Window* root_window : root_windows) {
    if (root_window->ContainsPointInRoot(
            root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot()))
      return root_window;
  }
  return nullptr;
}

}  // namespace

// The content mask provides a clipping layer for the magnification window so we
// can show a circular magnifier.
class PartialMagnificationController::ContentMask : public ui::LayerDelegate {
 public:
  // If |stroke| is true, the circle will be a stroke. This is useful if we wish
  // to clip a border.
  ContentMask(bool stroke, gfx::Size mask_bounds)
      : layer_(ui::LAYER_TEXTURED), stroke_(stroke) {
    layer_.set_delegate(this);
    layer_.SetFillsBoundsOpaquely(false);
    layer_.SetBounds(gfx::Rect(mask_bounds));
  }

  ~ContentMask() override { layer_.set_delegate(nullptr); }

  ui::Layer* layer() { return &layer_; }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, layer()->size());

    SkPaint paint;
    paint.setAlpha(255);
    paint.setAntiAlias(true);
    paint.setStrokeWidth(kBorderSize);
    paint.setStyle(stroke_ ? SkPaint::kStroke_Style : SkPaint::kFill_Style);

    gfx::Rect rect(layer()->bounds().size());
    recorder.canvas()->DrawCircle(rect.CenterPoint(),
                                  rect.width() / 2 - kBorderSize / 2, paint);
  }

  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override {}

  void OnDeviceScaleFactorChanged(float device_scale_factor) override {
    // Redrawing will take care of scale factor change.
  }

  base::Closure PrepareForLayerBoundsChange() override {
    return base::Closure();
  }

  ui::Layer layer_;
  bool stroke_;

  DISALLOW_COPY_AND_ASSIGN(ContentMask);
};

// The border renderer draws the border as well as outline on both the outer and
// inner radius to increase visibility.
class PartialMagnificationController::BorderRenderer
    : public ui::LayerDelegate {
 public:
  explicit BorderRenderer(const gfx::Rect& magnifier_bounds)
      : magnifier_bounds_(magnifier_bounds) {}

  ~BorderRenderer() override {}

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, magnifier_bounds_.size());

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);

    const int magnifier_radius = magnifier_bounds_.width() / 2;
    // Draw the inner border.
    paint.setStrokeWidth(kBorderSize);
    paint.setColor(kBorderColor);
    recorder.canvas()->DrawCircle(magnifier_bounds_.CenterPoint(),
                                  magnifier_radius - kBorderSize / 2, paint);

    // Draw border outer outline and then draw the border inner outline.
    paint.setStrokeWidth(kBorderOutlineThickness);
    paint.setColor(kBorderOutlineColor);
    recorder.canvas()->DrawCircle(
        magnifier_bounds_.CenterPoint(),
        magnifier_radius - kBorderOutlineThickness / 2, paint);
    recorder.canvas()->DrawCircle(
        magnifier_bounds_.CenterPoint(),
        magnifier_radius - kBorderSize + kBorderOutlineThickness / 2, paint);
  }

  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override {}

  void OnDeviceScaleFactorChanged(float device_scale_factor) override {}

  base::Closure PrepareForLayerBoundsChange() override {
    return base::Closure();
  }

  gfx::Rect magnifier_bounds_;

  DISALLOW_COPY_AND_ASSIGN(BorderRenderer);
};

PartialMagnificationController::PartialMagnificationController() {
  Shell::GetInstance()->AddPreTargetHandler(this);
}

PartialMagnificationController::~PartialMagnificationController() {
  CloseMagnifierWindow();

  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void PartialMagnificationController::SetEnabled(bool enabled) {
  is_enabled_ = enabled;
  SetActive(false);
}

void PartialMagnificationController::SwitchTargetRootWindowIfNeeded(
    aura::Window* new_root_window) {
  if (host_widget_ &&
      new_root_window == host_widget_->GetNativeView()->GetRootWindow())
    return;

  if (!new_root_window)
    new_root_window = GetCurrentRootWindow();

  if (is_enabled_ && is_active_) {
    CloseMagnifierWindow();
    CreateMagnifierWindow(new_root_window);
  }
}

void PartialMagnificationController::OnMouseEvent(ui::MouseEvent* event) {
  OnLocatedEvent(event, event->pointer_details());
}

void PartialMagnificationController::OnTouchEvent(ui::TouchEvent* event) {
  OnLocatedEvent(event, event->pointer_details());
}

void PartialMagnificationController::OnWindowDestroying(aura::Window* window) {
  CloseMagnifierWindow();

  aura::Window* new_root_window = GetCurrentRootWindow();
  if (new_root_window != window)
    SwitchTargetRootWindowIfNeeded(new_root_window);
}

void PartialMagnificationController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, host_widget_);
  RemoveZoomWidgetObservers();
  host_widget_ = nullptr;
}

void PartialMagnificationController::SetActive(bool active) {
  // Fail if we're trying to activate while disabled.
  DCHECK(is_enabled_ || !active);

  is_active_ = active;
  if (is_active_) {
    CreateMagnifierWindow(GetCurrentRootWindow());
  } else {
    CloseMagnifierWindow();
  }
}

void PartialMagnificationController::OnLocatedEvent(
    ui::LocatedEvent* event,
    const ui::PointerDetails& pointer_details) {
  if (!is_enabled_)
    return;

  if (pointer_details.pointer_type != ui::EventPointerType::POINTER_TYPE_PEN)
    return;

  // Compute the event location in screen space.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* event_root = target->GetRootWindow();
  gfx::Point screen_point = event->root_location();
  wm::ConvertPointToScreen(event_root, &screen_point);

  // If the stylus is pressed on the palette icon or widget, do not activate.
  if (event->type() == ui::ET_MOUSE_PRESSED &&
      !PaletteContainsPointInScreen(screen_point)) {
    SetActive(true);
  }

  if (event->type() == ui::ET_MOUSE_RELEASED)
    SetActive(false);

  if (!is_active_)
    return;

  // If the previous root window was detached host_widget_ will be null;
  // reconstruct it. We also need to change the root window if the cursor has
  // crossed display boundries.
  SwitchTargetRootWindowIfNeeded(GetCurrentRootWindow());

  // If that failed for any reason return.
  if (!host_widget_) {
    SetActive(false);
    return;
  }

  // Remap point from where it was captured to the display it is actually on.
  gfx::Point point = event->root_location();
  aura::Window::ConvertPointToTarget(
      event_root, host_widget_->GetNativeView()->GetRootWindow(), &point);
  host_widget_->SetBounds(GetBounds(point));

  // If the stylus is over the palette icon or widget, do not consume the event.
  if (!PaletteContainsPointInScreen(screen_point))
    event->StopPropagation();
}

void PartialMagnificationController::CreateMagnifierWindow(
    aura::Window* root_window) {
  if (host_widget_ || !root_window)
    return;

  root_window->AddObserver(this);

  gfx::Point mouse(
      root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot());

  host_widget_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.accept_events = false;
  params.bounds = GetBounds(mouse);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = root_window;
  host_widget_->Init(params);
  host_widget_->set_focus_on_creation(false);
  host_widget_->Show();

  aura::Window* window = host_widget_->GetNativeView();
  window->SetName(kPartialMagniferWindowName);

  ui::Layer* root_layer = host_widget_->GetNativeView()->layer();

  zoom_layer_.reset(new ui::Layer(ui::LayerType::LAYER_SOLID_COLOR));
  zoom_layer_->SetBounds(gfx::Rect(GetWindowSize()));
  zoom_layer_->SetBackgroundZoom(kMagnificationScale, kZoomInset);
  root_layer->Add(zoom_layer_.get());

  border_layer_.reset(new ui::Layer(ui::LayerType::LAYER_TEXTURED));
  border_layer_->SetBounds(gfx::Rect(GetWindowSize()));
  border_renderer_.reset(new BorderRenderer(gfx::Rect(GetWindowSize())));
  border_layer_->set_delegate(border_renderer_.get());
  root_layer->Add(border_layer_.get());

  border_mask_.reset(new ContentMask(true, GetWindowSize()));
  border_layer_->SetMaskLayer(border_mask_->layer());

  zoom_mask_.reset(new ContentMask(false, GetWindowSize()));
  zoom_layer_->SetMaskLayer(zoom_mask_->layer());

  host_widget_->AddObserver(this);
}

void PartialMagnificationController::CloseMagnifierWindow() {
  if (host_widget_) {
    RemoveZoomWidgetObservers();
    host_widget_->Close();
    host_widget_ = nullptr;
  }
}

void PartialMagnificationController::RemoveZoomWidgetObservers() {
  DCHECK(host_widget_);
  host_widget_->RemoveObserver(this);
  aura::Window* root_window = host_widget_->GetNativeView()->GetRootWindow();
  DCHECK(root_window);
  root_window->RemoveObserver(this);
}

}  // namespace ash
