// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host_platform.h"

#include <utility>

#include "ash/host/root_window_transformer.h"
#include "ash/host/transformer_helper.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "services/ui/public/cpp/input_devices/input_device_controller_client.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/mus/input_method_mus.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_sink.h"
#include "ui/events/null_event_targeter.h"
#include "ui/events/ozone/chromeos/cursor_controller.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"
#include "ui/platform_window/mojo/ime_type_converters.h"
#include "ui/platform_window/platform_ime_controller.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/text_input_state.h"

namespace ash {

AshWindowTreeHostPlatform::AshWindowTreeHostPlatform(
    ui::PlatformWindowInitProperties properties)
    : aura::WindowTreeHostPlatform(std::move(properties)),
      transformer_helper_(this) {
  transformer_helper_.Init();
  InitInputMethodIfNecessary();
}

AshWindowTreeHostPlatform::AshWindowTreeHostPlatform()
    : transformer_helper_(this) {
  CreateCompositor();
  transformer_helper_.Init();
  InitInputMethodIfNecessary();
}

AshWindowTreeHostPlatform::~AshWindowTreeHostPlatform() = default;

void AshWindowTreeHostPlatform::ConfineCursorToRootWindow() {
  if (!allow_confine_cursor())
    return;

  // We want to limit the cursor to what is visible, which is the size of the
  // compositor. |GetBoundsInPixels()| may include pixels that are not used.
  // See https://crbug.com/843354
  gfx::Rect confined_bounds(GetCompositorSizeInPixels());
  confined_bounds.Inset(transformer_helper_.GetHostInsets());
  last_cursor_confine_bounds_in_pixels_ = confined_bounds;
  platform_window()->ConfineCursorToBounds(confined_bounds);
}

void AshWindowTreeHostPlatform::ConfineCursorToBoundsInRoot(
    const gfx::Rect& bounds_in_root) {
  if (!allow_confine_cursor())
    return;

  gfx::RectF bounds_f(bounds_in_root);
  GetRootTransform().TransformRect(&bounds_f);
  last_cursor_confine_bounds_in_pixels_ = gfx::ToEnclosingRect(bounds_f);
  platform_window()->ConfineCursorToBounds(
      last_cursor_confine_bounds_in_pixels_);
}

gfx::Rect AshWindowTreeHostPlatform::GetLastCursorConfineBoundsInPixels()
    const {
  return last_cursor_confine_bounds_in_pixels_;
}

void AshWindowTreeHostPlatform::SetCursorConfig(
    const display::Display& display,
    display::Display::Rotation rotation) {
  // Scale all motion on High-DPI displays.
  float scale = display.device_scale_factor();

  if (!display.IsInternal())
    scale *= ui::mojom::kCursorMultiplierForExternalDisplays;

  ui::CursorController::GetInstance()->SetCursorConfigForWindow(
      GetAcceleratedWidget(), rotation, scale);
}

void AshWindowTreeHostPlatform::ClearCursorConfig() {
  ui::CursorController::GetInstance()->ClearCursorConfigForWindow(
      GetAcceleratedWidget());
}

void AshWindowTreeHostPlatform::UpdateTextInputState(
    ui::mojom::TextInputStatePtr state) {
  SetTextInputState(std::move(state));
}

void AshWindowTreeHostPlatform::UpdateImeVisibility(
    bool visible,
    ui::mojom::TextInputStatePtr state) {
  SetImeVisibility(visible, std::move(state));
}

void AshWindowTreeHostPlatform::SetRootWindowTransformer(
    std::unique_ptr<RootWindowTransformer> transformer) {
  transformer_helper_.SetRootWindowTransformer(std::move(transformer));
  ConfineCursorToRootWindow();
}

gfx::Insets AshWindowTreeHostPlatform::GetHostInsets() const {
  return transformer_helper_.GetHostInsets();
}

aura::WindowTreeHost* AshWindowTreeHostPlatform::AsWindowTreeHost() {
  return this;
}

void AshWindowTreeHostPlatform::PrepareForShutdown() {
  // Block the root window from dispatching events because it is weird for a
  // ScreenPositionClient not to be attached to the root window and for
  // ui::EventHandlers to be unable to convert the event's location to screen
  // coordinates.
  window()->SetEventTargeter(
      std::unique_ptr<ui::EventTargeter>(new ui::NullEventTargeter));

  // Do anything platform specific necessary before shutdown (eg. stop
  // listening for configuration XEvents).
  platform_window()->PrepareForShutdown();
}

void AshWindowTreeHostPlatform::SetRootTransform(
    const gfx::Transform& transform) {
  transformer_helper_.SetTransform(transform);
}

gfx::Transform AshWindowTreeHostPlatform::GetRootTransform() const {
  return transformer_helper_.GetTransform();
}

gfx::Transform AshWindowTreeHostPlatform::GetInverseRootTransform() const {
  return transformer_helper_.GetInverseTransform();
}

gfx::Rect AshWindowTreeHostPlatform::GetTransformedRootWindowBoundsInPixels(
    const gfx::Size& host_size_in_pixels) const {
  return transformer_helper_.GetTransformedWindowBounds(host_size_in_pixels);
}

void AshWindowTreeHostPlatform::OnCursorVisibilityChangedNative(bool show) {
  SetTapToClickPaused(!show);
}

void AshWindowTreeHostPlatform::SetBoundsInPixels(
    const gfx::Rect& bounds,
    const viz::LocalSurfaceId& local_surface_id) {
  WindowTreeHostPlatform::SetBoundsInPixels(bounds, local_surface_id);
  ConfineCursorToRootWindow();
}

gfx::Size AshWindowTreeHostPlatform::GetCompositorSizeInPixels() const {
  // For Chrome OS, the platform window size may be slightly different from the
  // compositor pixel size. This is to prevent any trailing 1px line at the
  // right or bottom edge due to rounding. This means we may not be using ALL
  // the pixels on a display, however this is a temporary fix until we figure
  // out a way to prevent these rounding artifacts.
  // See https://crbug.com/843354 and https://crbug.com/862424 for more info.
  if (device_scale_factor() == 1.f)
    return GetBoundsInPixels().size();
  return gfx::ScaleToRoundedSize(
      gfx::ScaleToFlooredSize(GetBoundsInPixels().size(),
                              1.f / device_scale_factor()),
      device_scale_factor());
}

void AshWindowTreeHostPlatform::OnBoundsChanged(const gfx::Rect& new_bounds) {
  // We need to recompute the bounds in pixels based on the DIP size. This is a
  // temporary fix needed because the root layer has the bounds in DIP which
  // when scaled by the compositor does not match the display bounds in pixels.
  // So we need to change the display bounds to match the root layer's scaled
  // size.
  // See https://crbug.com/843354 for more info.
  const float new_scale = ui::GetScaleFactorForNativeView(window());
  WindowTreeHostPlatform::OnBoundsChanged(gfx::ScaleToRoundedRect(
      gfx::ScaleToEnclosedRect(new_bounds, 1.f / new_scale), new_scale));
}

void AshWindowTreeHostPlatform::DispatchEvent(ui::Event* event) {
  TRACE_EVENT0("input", "AshWindowTreeHostPlatform::DispatchEvent");
  if (event->IsLocatedEvent())
    TranslateLocatedEvent(static_cast<ui::LocatedEvent*>(event));
  SendEventToSink(event);
}

void AshWindowTreeHostPlatform::InitInputMethodIfNecessary() {
  if (!base::FeatureList::IsEnabled(features::kMash))
    return;

  input_method_ = std::make_unique<aura::InputMethodMus>(this, this);
  input_method_->Init(Shell::Get()->connector());
  SetSharedInputMethod(input_method_.get());
}

void AshWindowTreeHostPlatform::SetTapToClickPaused(bool state) {
  ui::InputDeviceControllerClient* input_device_controller_client =
      Shell::Get()->shell_delegate()->GetInputDeviceControllerClient();
  if (!input_device_controller_client)
    return;  // Happens in tests.

  // Temporarily pause tap-to-click when the cursor is hidden.
  input_device_controller_client->SetTapToClickPaused(state);
}

void AshWindowTreeHostPlatform::SetTextInputState(
    ui::mojom::TextInputStatePtr state) {
  ui::PlatformImeController* ime =
      platform_window()->GetPlatformImeController();
  if (ime)
    ime->UpdateTextInputState(state.To<ui::TextInputState>());
}

void AshWindowTreeHostPlatform::SetImeVisibility(
    bool visible,
    ui::mojom::TextInputStatePtr state) {
  if (!state.is_null())
    SetTextInputState(std::move(state));

  ui::PlatformImeController* ime =
      platform_window()->GetPlatformImeController();
  if (ime)
    ime->SetImeVisibility(visible);
}

}  // namespace ash
