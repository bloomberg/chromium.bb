// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host_platform.h"

#include <utility>

#include "ash/host/root_window_transformer.h"
#include "ash/host/transformer_helper.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/trace_event/trace_event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/events/event_sink.h"
#include "ui/events/null_event_targeter.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/transform.h"
#include "ui/platform_window/platform_window.h"

#if defined(USE_OZONE)
#include "services/ui/public/cpp/input_devices/input_device_controller_client.h"
#endif

namespace ash {

AshWindowTreeHostPlatform::AshWindowTreeHostPlatform(
    const gfx::Rect& initial_bounds)
    : aura::WindowTreeHostPlatform(initial_bounds), transformer_helper_(this) {
  transformer_helper_.Init();
}

AshWindowTreeHostPlatform::AshWindowTreeHostPlatform()
    : transformer_helper_(this) {
  CreateCompositor();
  transformer_helper_.Init();
}

AshWindowTreeHostPlatform::~AshWindowTreeHostPlatform() {}

void AshWindowTreeHostPlatform::ToggleFullScreen() {
  NOTIMPLEMENTED();
}

bool AshWindowTreeHostPlatform::ConfineCursorToRootWindow() {
  gfx::Rect confined_bounds(GetBoundsInPixels().size());
  confined_bounds.Inset(transformer_helper_.GetHostInsets());
  platform_window()->ConfineCursorToBounds(confined_bounds);
  return true;
}

void AshWindowTreeHostPlatform::UnConfineCursor() {
  NOTIMPLEMENTED();
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

void AshWindowTreeHostPlatform::UpdateRootWindowSizeInPixels(
    const gfx::Size& host_size_in_pixels) {
  transformer_helper_.UpdateWindowSize(host_size_in_pixels);
}

void AshWindowTreeHostPlatform::OnCursorVisibilityChangedNative(bool show) {
  SetTapToClickPaused(!show);
}

void AshWindowTreeHostPlatform::SetBoundsInPixels(const gfx::Rect& bounds) {
  WindowTreeHostPlatform::SetBoundsInPixels(bounds);
  ConfineCursorToRootWindow();
}

void AshWindowTreeHostPlatform::DispatchEvent(ui::Event* event) {
  TRACE_EVENT0("input", "AshWindowTreeHostPlatform::DispatchEvent");
  if (event->IsLocatedEvent())
    TranslateLocatedEvent(static_cast<ui::LocatedEvent*>(event));
  SendEventToSink(event);
}

void AshWindowTreeHostPlatform::SetTapToClickPaused(bool state) {
#if defined(USE_OZONE)
  ui::InputDeviceControllerClient* input_device_controller_client =
      Shell::Get()->shell_delegate()->GetInputDeviceControllerClient();
  if (!input_device_controller_client)
    return;  // Happens in tests.

  // Temporarily pause tap-to-click when the cursor is hidden.
  input_device_controller_client->SetTapToClickPaused(state);
#endif
}

}  // namespace ash
