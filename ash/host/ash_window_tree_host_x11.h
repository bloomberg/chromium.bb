// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_ASH_WINDOW_TREE_HOST_X11_H_
#define ASH_HOST_ASH_WINDOW_TREE_HOST_X11_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/transformer_helper.h"
#include "base/macros.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_tree_host_x11.h"

namespace ash {
class RootWindowTransformer;

class ASH_EXPORT AshWindowTreeHostX11 : public AshWindowTreeHost,
                                        public aura::WindowTreeHostX11,
                                        public aura::EnvObserver {
 public:
  explicit AshWindowTreeHostX11(const gfx::Rect& initial_bounds);
  ~AshWindowTreeHostX11() override;

 private:
  // AshWindowTreeHost:
  void ToggleFullScreen() override;
  bool ConfineCursorToRootWindow() override;
  void UnConfineCursor() override;
  void SetRootWindowTransformer(
      std::unique_ptr<RootWindowTransformer> transformer) override;
  gfx::Insets GetHostInsets() const override;
  aura::WindowTreeHost* AsWindowTreeHost() override;
  void PrepareForShutdown() override;

  // aura::WindowTreehost:
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Transform GetRootTransform() const override;
  void SetRootTransform(const gfx::Transform& transform) override;
  gfx::Transform GetInverseRootTransform() const override;
  void UpdateRootWindowSizeInPixels(
      const gfx::Size& host_size_in_pixels) override;
  void OnCursorVisibilityChangedNative(bool show) override;

  // aura::WindowTreeHostX11:
  void OnConfigureNotify() override;
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  void TranslateAndDispatchLocatedEvent(ui::LocatedEvent* event) override;

  // EnvObserver overrides.
  void OnWindowInitialized(aura::Window* window) override;
  void OnHostInitialized(aura::WindowTreeHost* host) override;

  // ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) override;

  // Set the CrOS touchpad "tap paused" property. It is used to temporarily
  // turn off the Tap-to-click feature when the mouse pointer is invisible.
  void SetCrOSTapPaused(bool state);

  std::unique_ptr<XID[]> pointer_barriers_;

  TransformerHelper transformer_helper_;

  DISALLOW_COPY_AND_ASSIGN(AshWindowTreeHostX11);
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_X11_H_
