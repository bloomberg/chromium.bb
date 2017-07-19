// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/ash_window_tree_host_mus.h"

#include "ash/host/root_window_transformer.h"
#include "ash/host/transformer_helper.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/window.h"
#include "ui/events/event_sink.h"
#include "ui/events/null_event_targeter.h"

#if defined(USE_OZONE)
#include "services/ui/public/cpp/input_devices/input_device_controller_client.h"
#endif

namespace ash {

AshWindowTreeHostMus::AshWindowTreeHostMus(
    aura::WindowTreeHostMusInitParams init_params)
    : aura::WindowTreeHostMus(std::move(init_params)),
      transformer_helper_(base::MakeUnique<TransformerHelper>(this)) {
  transformer_helper_->Init();
}

AshWindowTreeHostMus::~AshWindowTreeHostMus() {}

bool AshWindowTreeHostMus::ConfineCursorToRootWindow() {
  // TODO: when implementing see implementation in AshWindowTreeHostPlatform
  // for how it uses |transformer_helper_|. http://crbug.com/746054.
  NOTIMPLEMENTED();
  return true;
}

void AshWindowTreeHostMus::UnConfineCursor() {
  NOTIMPLEMENTED();
}

void AshWindowTreeHostMus::SetRootWindowTransformer(
    std::unique_ptr<RootWindowTransformer> transformer) {
  transformer_helper_->SetRootWindowTransformer(std::move(transformer));
  ConfineCursorToRootWindow();
}

gfx::Insets AshWindowTreeHostMus::GetHostInsets() const {
  return transformer_helper_->GetHostInsets();
}

aura::WindowTreeHost* AshWindowTreeHostMus::AsWindowTreeHost() {
  return this;
}

void AshWindowTreeHostMus::PrepareForShutdown() {
  // WindowEventDispatcher may have pending events that need to be processed.
  // At the time this function is called the WindowTreeHost and Window are in
  // a semi-shutdown state. Reset the targeter so that the current targeter
  // doesn't attempt to process events while in this state, which would likely
  // crash.
  std::unique_ptr<ui::NullEventTargeter> null_event_targeter =
      base::MakeUnique<ui::NullEventTargeter>();
  window()->SetEventTargeter(std::move(null_event_targeter));
}

void AshWindowTreeHostMus::RegisterMirroringHost(
    AshWindowTreeHost* mirroring_ash_host) {
  // This should not be called, but it is because mirroring isn't wired up for
  // mus. Once that is done, this should be converted to a NOTREACHED.
  NOTIMPLEMENTED();
}

#if defined(USE_OZONE)
void AshWindowTreeHostMus::SetCursorConfig(
    const display::Display& display,
    display::Display::Rotation rotation) {
  // Nothing to do here, mus takes care of this.
}

void AshWindowTreeHostMus::ClearCursorConfig() {
  // Nothing to do here, mus takes care of this.
}
#endif

void AshWindowTreeHostMus::SetRootTransform(const gfx::Transform& transform) {
  transformer_helper_->SetTransform(transform);
}

gfx::Transform AshWindowTreeHostMus::GetRootTransform() const {
  return transformer_helper_->GetTransform();
}

gfx::Transform AshWindowTreeHostMus::GetInverseRootTransform() const {
  return transformer_helper_->GetInverseTransform();
}

void AshWindowTreeHostMus::UpdateRootWindowSizeInPixels(
    const gfx::Size& host_size_in_pixels) {
  transformer_helper_->UpdateWindowSize(host_size_in_pixels);
}

void AshWindowTreeHostMus::OnCursorVisibilityChangedNative(bool show) {
#if defined(USE_OZONE)
  ui::InputDeviceControllerClient* input_device_controller_client =
      Shell::Get()->shell_delegate()->GetInputDeviceControllerClient();
  if (!input_device_controller_client)
    return;  // Happens in tests.

  // Temporarily pause tap-to-click when the cursor is hidden.
  input_device_controller_client->SetTapToClickPaused(!show);
#endif
}

}  // namespace ash
