// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_ASH_WINDOW_TREE_HOST_PLATFORM_H_
#define ASH_HOST_ASH_WINDOW_TREE_HOST_PLATFORM_H_

#include "ash/ash_export.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/transformer_helper.h"
#include "ui/aura/window_tree_host_platform.h"

namespace ash {

class ASH_EXPORT AshWindowTreeHostPlatform
    : public AshWindowTreeHost,
      public aura::WindowTreeHostPlatform {
 public:
  explicit AshWindowTreeHostPlatform(const gfx::Rect& initial_bounds);
  ~AshWindowTreeHostPlatform() override;

 protected:
  AshWindowTreeHostPlatform();

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

  // ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) override;

 private:
  // Temporarily disable the tap-to-click feature. Used on CrOS.
  void SetTapToClickPaused(bool state);

  TransformerHelper transformer_helper_;

  DISALLOW_COPY_AND_ASSIGN(AshWindowTreeHostPlatform);
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_PLATFORM_H_
