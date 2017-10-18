// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_ASH_WINDOW_TREE_HOST_PLATFORM_H_
#define ASH_HOST_ASH_WINDOW_TREE_HOST_PLATFORM_H_

#include <memory>

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
  bool ConfineCursorToRootWindow() override;
  void SetRootWindowTransformer(
      std::unique_ptr<RootWindowTransformer> transformer) override;
  gfx::Insets GetHostInsets() const override;
  aura::WindowTreeHost* AsWindowTreeHost() override;
  void PrepareForShutdown() override;
  void SetCursorConfig(const display::Display& display,
                       display::Display::Rotation rotation) override;
  void ClearCursorConfig() override;

  // aura::WindowTreeHostPlatform:
  void SetRootTransform(const gfx::Transform& transform) override;
  gfx::Transform GetRootTransform() const override;
  gfx::Transform GetInverseRootTransform() const override;
  void UpdateRootWindowSizeInPixels(
      const gfx::Size& host_size_in_pixels) override;
  void OnCursorVisibilityChangedNative(bool show) override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  void DispatchEvent(ui::Event* event) override;

 private:
  // Temporarily disable the tap-to-click feature. Used on CrOS.
  void SetTapToClickPaused(bool state);

  // ui::PlatformWindowDelegate:
  void OnBoundsChanged(const gfx::Rect& new_bounds) override;

  TransformerHelper transformer_helper_;

  DISALLOW_COPY_AND_ASSIGN(AshWindowTreeHostPlatform);
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_PLATFORM_H_
