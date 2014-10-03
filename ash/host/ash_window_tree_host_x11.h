// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_ASH_WINDOW_TREE_HOST_X11_H_
#define ASH_HOST_ASH_WINDOW_TREE_HOST_X11_H_

#include "ash/ash_export.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/transformer_helper.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_tree_host_x11.h"

namespace ash {
class RootWindowTransformer;
class MouseCursorEventFilter;

class ASH_EXPORT AshWindowTreeHostX11 : public AshWindowTreeHost,
                                        public aura::WindowTreeHostX11,
                                        public aura::EnvObserver {
 public:
  explicit AshWindowTreeHostX11(const gfx::Rect& initial_bounds);
  virtual ~AshWindowTreeHostX11();

 private:
  // AshWindowTreeHost:
  virtual void ToggleFullScreen() override;
  virtual bool ConfineCursorToRootWindow() override;
  virtual void UnConfineCursor() override;
  virtual void SetRootWindowTransformer(
      scoped_ptr<RootWindowTransformer> transformer) override;
  virtual gfx::Insets GetHostInsets() const override;
  virtual aura::WindowTreeHost* AsWindowTreeHost() override;
  virtual void UpdateDisplayID(int64 id1, int64 id2) override;
  virtual void PrepareForShutdown() override;

  // aura::WindowTreehost:
  virtual void SetBounds(const gfx::Rect& bounds) override;
  virtual gfx::Transform GetRootTransform() const override;
  virtual void SetRootTransform(const gfx::Transform& transform) override;
  virtual gfx::Transform GetInverseRootTransform() const override;
  virtual void UpdateRootWindowSize(const gfx::Size& host_size) override;
  virtual void OnCursorVisibilityChangedNative(bool show) override;

  // aura::WindowTreeHostX11:
  virtual void OnConfigureNotify() override;
  virtual bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  virtual void TranslateAndDispatchLocatedEvent(ui::LocatedEvent* event)
      override;

  // EnvObserver overrides.
  virtual void OnWindowInitialized(aura::Window* window) override;
  virtual void OnHostInitialized(aura::WindowTreeHost* host) override;

#if defined(OS_CHROMEOS)
  // Set the CrOS touchpad "tap paused" property. It is used to temporarily
  // turn off the Tap-to-click feature when the mouse pointer is invisible.
  void SetCrOSTapPaused(bool state);
#endif

  scoped_ptr<XID[]> pointer_barriers_;

  TransformerHelper transformer_helper_;

  // The display IDs associated with this root window.
  // In single monitor or extended mode dual monitor case, the root window
  // is associated with one display.
  // In mirror mode dual monitors case, the root window is associated with
  // both displays.
  std::pair<int64, int64> display_ids_;

  DISALLOW_COPY_AND_ASSIGN(AshWindowTreeHostX11);
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_X11_H_
