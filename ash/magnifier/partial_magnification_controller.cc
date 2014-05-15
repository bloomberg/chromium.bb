// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/partial_magnification_controller.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/screen.h"
#include "ui/compositor/layer.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/compound_event_filter.h"

namespace {

const float kMinPartialMagnifiedScaleThreshold = 1.1f;

// Number of pixels to make the border of the magnified area.
const int kZoomInset = 16;

// Width of the magnified area.
const int kMagnifierWidth = 200;

// Height of the magnified area.
const int kMagnifierHeight = 200;

// Name of the magnifier window.
const char kPartialMagniferWindowName[] = "PartialMagnifierWindow";

}  // namespace

namespace ash {

PartialMagnificationController::PartialMagnificationController()
    : is_on_zooming_(false),
      is_enabled_(false),
      scale_(kNonPartialMagnifiedScale),
      zoom_widget_(NULL) {
  Shell::GetInstance()->AddPreTargetHandler(this);
}

PartialMagnificationController::~PartialMagnificationController() {
  CloseMagnifierWindow();

  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void PartialMagnificationController::SetScale(float scale) {
  if (!is_enabled_)
    return;

  scale_ = scale;

  if (IsPartialMagnified()) {
    CreateMagnifierWindow();
  } else {
    CloseMagnifierWindow();
  }
}

void PartialMagnificationController::SetEnabled(bool enabled) {
  if (enabled) {
    is_enabled_ = enabled;
    SetScale(kDefaultPartialMagnifiedScale);
  } else {
    SetScale(kNonPartialMagnifiedScale);
    is_enabled_ = enabled;
  }
}

////////////////////////////////////////////////////////////////////////////////
// PartialMagnificationController: ui::EventHandler implementation

void PartialMagnificationController::OnMouseEvent(ui::MouseEvent* event) {
  if (IsPartialMagnified() && event->type() == ui::ET_MOUSE_MOVED) {
    aura::Window* target = static_cast<aura::Window*>(event->target());
    aura::Window* current_root = target->GetRootWindow();
    // TODO(zork): Handle the case where the event is captured on a different
    // display, such as when a menu is opened.
    gfx::Rect root_bounds = current_root->bounds();

    if (root_bounds.Contains(event->root_location())) {
      SwitchTargetRootWindow(current_root);

      OnMouseMove(event->root_location());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// PartialMagnificationController: aura::WindowObserver implementation

void PartialMagnificationController::OnWindowDestroying(
    aura::Window* window) {
  CloseMagnifierWindow();

  aura::Window* new_root_window = GetCurrentRootWindow();
  if (new_root_window != window)
    SwitchTargetRootWindow(new_root_window);
}

void PartialMagnificationController::OnWidgetDestroying(
    views::Widget* widget) {
  DCHECK_EQ(widget, zoom_widget_);
  RemoveZoomWidgetObservers();
  zoom_widget_ = NULL;
}

void PartialMagnificationController::OnMouseMove(
    const gfx::Point& location_in_root) {
  gfx::Point origin(location_in_root);

  origin.Offset(-kMagnifierWidth / 2, -kMagnifierHeight / 2);

  if (zoom_widget_) {
    zoom_widget_->SetBounds(gfx::Rect(origin.x(), origin.y(),
                                      kMagnifierWidth, kMagnifierHeight));
  }
}

bool PartialMagnificationController::IsPartialMagnified() const {
  return scale_ >= kMinPartialMagnifiedScaleThreshold;
}

void PartialMagnificationController::CreateMagnifierWindow() {
  if (zoom_widget_)
    return;

  aura::Window* root_window = GetCurrentRootWindow();
  if (!root_window)
    return;

  root_window->AddObserver(this);

  gfx::Point mouse(
      root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot());

  zoom_widget_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.accept_events = false;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = root_window;
  zoom_widget_->Init(params);
  zoom_widget_->SetBounds(gfx::Rect(mouse.x() - kMagnifierWidth / 2,
                                    mouse.y() - kMagnifierHeight / 2,
                                    kMagnifierWidth, kMagnifierHeight));
  zoom_widget_->set_focus_on_creation(false);
  zoom_widget_->Show();

  aura::Window* window = zoom_widget_->GetNativeView();
  window->SetName(kPartialMagniferWindowName);

  zoom_widget_->GetNativeView()->layer()->SetBounds(
      gfx::Rect(0, 0,
                kMagnifierWidth,
                kMagnifierHeight));
  zoom_widget_->GetNativeView()->layer()->SetBackgroundZoom(
      scale_,
      kZoomInset);

  zoom_widget_->AddObserver(this);
}

void PartialMagnificationController::CloseMagnifierWindow() {
  if (zoom_widget_) {
    RemoveZoomWidgetObservers();
    zoom_widget_->Close();
    zoom_widget_ = NULL;
  }
}

void PartialMagnificationController::RemoveZoomWidgetObservers() {
  DCHECK(zoom_widget_);
  zoom_widget_->RemoveObserver(this);
  aura::Window* root_window =
      zoom_widget_->GetNativeView()->GetRootWindow();
  DCHECK(root_window);
  root_window->RemoveObserver(this);
}

void PartialMagnificationController::SwitchTargetRootWindow(
    aura::Window* new_root_window) {
  if (zoom_widget_ &&
      new_root_window == zoom_widget_->GetNativeView()->GetRootWindow())
    return;

  CloseMagnifierWindow();

  // Recreate the magnifier window by updating the scale factor.
  SetScale(GetScale());
}

aura::Window* PartialMagnificationController::GetCurrentRootWindow() {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (aura::Window::Windows::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    aura::Window* root_window = *iter;
    if (root_window->ContainsPointInRoot(
            root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot()))
      return root_window;
  }
  return NULL;
}

}  // namespace ash
