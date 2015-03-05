// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host.h"

#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/host/root_window_transformer.h"
#include "ash/host/transformer_helper.h"
#include "base/command_line.h"
#include "base/trace_event/trace_event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_ozone.h"
#include "ui/events/null_event_targeter.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/transform.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window.h"

namespace ash {
namespace {

class AshWindowTreeHostOzone : public AshWindowTreeHost,
                               public aura::WindowTreeHostOzone {
 public:
  explicit AshWindowTreeHostOzone(const gfx::Rect& initial_bounds);
  ~AshWindowTreeHostOzone() override;

 private:
  // AshWindowTreeHost:
  void ToggleFullScreen() override;
  bool ConfineCursorToRootWindow() override;
  void UnConfineCursor() override;
  void SetRootWindowTransformer(
      scoped_ptr<RootWindowTransformer> transformer) override;
  gfx::Insets GetHostInsets() const override;
  aura::WindowTreeHost* AsWindowTreeHost() override;
  void PrepareForShutdown() override;
  void SetRootTransform(const gfx::Transform& transform) override;
  gfx::Transform GetRootTransform() const override;
  gfx::Transform GetInverseRootTransform() const override;
  void UpdateRootWindowSize(const gfx::Size& host_size) override;
  void OnCursorVisibilityChangedNative(bool show) override;
  void SetBounds(const gfx::Rect& bounds) override;
  void DispatchEvent(ui::Event* event) override;

  // Temporarily disable the tap-to-click feature. Used on CrOS.
  void SetTapToClickPaused(bool state);

  TransformerHelper transformer_helper_;

  DISALLOW_COPY_AND_ASSIGN(AshWindowTreeHostOzone);
};

AshWindowTreeHostOzone::AshWindowTreeHostOzone(const gfx::Rect& initial_bounds)
    : aura::WindowTreeHostOzone(initial_bounds), transformer_helper_(this) {
}

AshWindowTreeHostOzone::~AshWindowTreeHostOzone() {
}

void AshWindowTreeHostOzone::ToggleFullScreen() {
  NOTIMPLEMENTED();
}

bool AshWindowTreeHostOzone::ConfineCursorToRootWindow() {
  gfx::Rect confined_bounds(GetBounds().size());
  confined_bounds.Inset(transformer_helper_.GetHostInsets());
  platform_window()->ConfineCursorToBounds(confined_bounds);
  return true;
}

void AshWindowTreeHostOzone::UnConfineCursor() {
  NOTIMPLEMENTED();
}

void AshWindowTreeHostOzone::SetRootWindowTransformer(
    scoped_ptr<RootWindowTransformer> transformer) {
  transformer_helper_.SetRootWindowTransformer(transformer.Pass());
  ConfineCursorToRootWindow();
}

gfx::Insets AshWindowTreeHostOzone::GetHostInsets() const {
  return transformer_helper_.GetHostInsets();
}

aura::WindowTreeHost* AshWindowTreeHostOzone::AsWindowTreeHost() {
  return this;
}

void AshWindowTreeHostOzone::PrepareForShutdown() {
  // Block the root window from dispatching events because it is weird for a
  // ScreenPositionClient not to be attached to the root window and for
  // ui::EventHandlers to be unable to convert the event's location to screen
  // coordinates.
  window()->SetEventTargeter(
      scoped_ptr<ui::EventTargeter>(new ui::NullEventTargeter));
}

void AshWindowTreeHostOzone::SetRootTransform(const gfx::Transform& transform) {
  transformer_helper_.SetTransform(transform);
}

gfx::Transform AshWindowTreeHostOzone::GetRootTransform() const {
  return transformer_helper_.GetTransform();
}

gfx::Transform AshWindowTreeHostOzone::GetInverseRootTransform() const {
  return transformer_helper_.GetInverseTransform();
}

void AshWindowTreeHostOzone::UpdateRootWindowSize(const gfx::Size& host_size) {
  transformer_helper_.UpdateWindowSize(host_size);
}

void AshWindowTreeHostOzone::OnCursorVisibilityChangedNative(bool show) {
  SetTapToClickPaused(!show);
}

void AshWindowTreeHostOzone::SetBounds(const gfx::Rect& bounds) {
  WindowTreeHostOzone::SetBounds(bounds);
  ConfineCursorToRootWindow();
}

void AshWindowTreeHostOzone::DispatchEvent(ui::Event* event) {
  TRACE_EVENT0("input", "AshWindowTreeHostOzone::DispatchEvent");
  if (event->IsLocatedEvent())
    TranslateLocatedEvent(static_cast<ui::LocatedEvent*>(event));
  SendEventToProcessor(event);
}

void AshWindowTreeHostOzone::SetTapToClickPaused(bool state) {
#if defined(OS_CHROMEOS)
  DCHECK(ui::OzonePlatform::GetInstance()->GetInputController());

  // Temporarily pause tap-to-click when the cursor is hidden.
  ui::OzonePlatform::GetInstance()->GetInputController()->SetTapToClickPaused(
      state);
#endif
}

}  // namespace

AshWindowTreeHost* AshWindowTreeHost::Create(
    const AshWindowTreeHostInitParams& init_params) {
  return new AshWindowTreeHostOzone(init_params.initial_bounds);
}

}  // namespace ash
