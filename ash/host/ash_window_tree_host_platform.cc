// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host_platform.h"

#include <utility>

#include "ash/host/root_window_transformer.h"
#include "ash/host/transformer_helper.h"
#include "ash/ime/input_method_event_handler.h"
#include "base/trace_event/trace_event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/events/event_processor.h"
#include "ui/events/null_event_targeter.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/transform.h"
#include "ui/platform_window/platform_window.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace ash {

AshWindowTreeHostPlatform::AshWindowTreeHostPlatform(
    const gfx::Rect& initial_bounds)
    : aura::WindowTreeHostPlatform(initial_bounds), transformer_helper_(this) {
  transformer_helper_.Init();
}

AshWindowTreeHostPlatform::AshWindowTreeHostPlatform()
    : transformer_helper_(this) {
  transformer_helper_.Init();
}

AshWindowTreeHostPlatform::~AshWindowTreeHostPlatform() {}

void AshWindowTreeHostPlatform::ToggleFullScreen() {
  NOTIMPLEMENTED();
}

bool AshWindowTreeHostPlatform::ConfineCursorToRootWindow() {
  gfx::Rect confined_bounds(GetBounds().size());
  confined_bounds.Inset(transformer_helper_.GetHostInsets());
  platform_window()->ConfineCursorToBounds(confined_bounds);
  return true;
}

void AshWindowTreeHostPlatform::UnConfineCursor() {
  NOTIMPLEMENTED();
}

void AshWindowTreeHostPlatform::SetRootWindowTransformer(
    scoped_ptr<RootWindowTransformer> transformer) {
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
      scoped_ptr<ui::EventTargeter>(new ui::NullEventTargeter));
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

void AshWindowTreeHostPlatform::UpdateRootWindowSize(
    const gfx::Size& host_size) {
  transformer_helper_.UpdateWindowSize(host_size);
}

void AshWindowTreeHostPlatform::OnCursorVisibilityChangedNative(bool show) {
  SetTapToClickPaused(!show);
}

void AshWindowTreeHostPlatform::SetBounds(const gfx::Rect& bounds) {
  WindowTreeHostPlatform::SetBounds(bounds);
  ConfineCursorToRootWindow();
}

void AshWindowTreeHostPlatform::DispatchEvent(ui::Event* event) {
  TRACE_EVENT0("input", "AshWindowTreeHostPlatform::DispatchEvent");
  if (event->IsLocatedEvent())
    TranslateLocatedEvent(static_cast<ui::LocatedEvent*>(event));
  SendEventToProcessor(event);
}

ui::EventDispatchDetails AshWindowTreeHostPlatform::DispatchKeyEventPostIME(
    ui::KeyEvent* event) {
  input_method_handler()->SetPostIME(true);
  ui::EventDispatchDetails details =
      event_processor()->OnEventFromSource(event);
  if (!details.dispatcher_destroyed)
    input_method_handler()->SetPostIME(false);
  return details;
}

void AshWindowTreeHostPlatform::SetTapToClickPaused(bool state) {
#if defined(USE_OZONE)
  DCHECK(ui::OzonePlatform::GetInstance()->GetInputController());

  // Temporarily pause tap-to-click when the cursor is hidden.
  ui::OzonePlatform::GetInstance()->GetInputController()->SetTapToClickPaused(
      state);
#endif
}

}  // namespace ash
