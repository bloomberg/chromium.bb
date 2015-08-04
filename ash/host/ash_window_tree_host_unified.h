// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_ASH_WINDOW_TREE_HOST_UNIFIED_H_
#define ASH_HOST_ASH_WINDOW_TREE_HOST_UNIFIED_H_

#include <vector>

#include "ash/host/ash_window_tree_host.h"
#include "ash/host/transformer_helper.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_source.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class Reflector;
}

namespace ash {
class DisplayInfo;

// A WTH used for unified desktop mode. This creates an offscreen
// compositor whose texture will be copied into each displays'
// compositor.
class AshWindowTreeHostUnified : public AshWindowTreeHost,
                                 public aura::WindowTreeHost,
                                 public aura::WindowObserver {
 public:
  explicit AshWindowTreeHostUnified(const gfx::Rect& initial_bounds);
  ~AshWindowTreeHostUnified() override;

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
  void RegisterMirroringHost(AshWindowTreeHost* mirroring_ash_host) override;

  // aura::WindowTreeHost:
  ui::EventSource* GetEventSource() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  void ShowImpl() override;
  void HideImpl() override;
  gfx::Rect GetBounds() const override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Transform GetRootTransform() const override;
  void SetRootTransform(const gfx::Transform& transform) override;
  gfx::Transform GetInverseRootTransform() const override;
  void UpdateRootWindowSize(const gfx::Size& host_size) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  gfx::Point GetLocationOnNativeScreen() const override;
  void SetCursorNative(gfx::NativeCursor cursor) override;
  void MoveCursorToNative(const gfx::Point& location) override;
  void OnCursorVisibilityChangedNative(bool show) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) override;

  std::vector<AshWindowTreeHost*> mirroring_hosts_;

  gfx::Rect bounds_;

  TransformerHelper transformer_helper_;

  DISALLOW_COPY_AND_ASSIGN(AshWindowTreeHostUnified);
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_UNIFIED_H_
