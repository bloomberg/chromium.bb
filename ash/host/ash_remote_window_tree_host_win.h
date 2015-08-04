// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_REMOTE_WINDOW_TREE_HOST_WIN_H_
#define ASH_HOST_REMOTE_WINDOW_TREE_HOST_WIN_H_

#include <windows.h>

#include "ash/ash_export.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/transformer_helper.h"
#include "ui/aura/remote_window_tree_host_win.h"

namespace ash {

class ASH_EXPORT AshRemoteWindowTreeHostWin
    : public AshWindowTreeHost,
      public aura::RemoteWindowTreeHostWin {
 public:
  explicit AshRemoteWindowTreeHostWin(HWND remote_hwnd);

 private:
  ~AshRemoteWindowTreeHostWin() override;

  // AshWindowTreeHost:
  void ToggleFullScreen() override;
  bool ConfineCursorToRootWindow() override;
  void UnConfineCursor() override;
  void SetRootWindowTransformer(
      scoped_ptr<RootWindowTransformer> transformer) override;
  gfx::Insets GetHostInsets() const override;
  aura::WindowTreeHost* AsWindowTreeHost() override;

  // WindowTreeHostWin:
  gfx::Transform GetRootTransform() const override;
  void SetRootTransform(const gfx::Transform& transform) override;
  gfx::Transform GetInverseRootTransform() const override;
  void UpdateRootWindowSize(const gfx::Size& host_size) override;

  // ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) override;

  TransformerHelper transformer_helper_;

  DISALLOW_COPY_AND_ASSIGN(AshRemoteWindowTreeHostWin);
};

}  // namespace ash

#endif  // ASH_HOST_REMOTE_WINDOW_TREE_HOST_WIN_H_
