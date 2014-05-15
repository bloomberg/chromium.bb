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
  virtual ~AshRemoteWindowTreeHostWin();

  // AshWindowTreeHost:
  virtual void ToggleFullScreen() OVERRIDE;
  virtual bool ConfineCursorToRootWindow() OVERRIDE;
  virtual void UnConfineCursor() OVERRIDE;
  virtual void SetRootWindowTransformer(
      scoped_ptr<RootWindowTransformer> transformer) OVERRIDE;
  virtual gfx::Insets GetHostInsets() const OVERRIDE;
  virtual aura::WindowTreeHost* AsWindowTreeHost() OVERRIDE;

  // WindowTreeHostWin:
  virtual gfx::Transform GetRootTransform() const OVERRIDE;
  virtual void SetRootTransform(const gfx::Transform& transform) OVERRIDE;
  virtual gfx::Transform GetInverseRootTransform() const OVERRIDE;
  virtual void UpdateRootWindowSize(const gfx::Size& host_size) OVERRIDE;

  TransformerHelper transformer_helper_;

  DISALLOW_COPY_AND_ASSIGN(AshRemoteWindowTreeHostWin);
};

}  // namespace ash

#endif  // ASH_HOST_REMOTE_WINDOW_TREE_HOST_WIN_H_
