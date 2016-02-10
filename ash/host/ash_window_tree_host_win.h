// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_ASH_WINDOW_TREE_HOST_WIN_H_
#define ASH_HOST_ASH_WINDOW_TREE_HOST_WIN_H_

#include "ash/host/ash_window_tree_host_platform.h"

namespace ash {

class AshWindowTreeHostWin : public AshWindowTreeHostPlatform {
 public:
  explicit AshWindowTreeHostWin(const gfx::Rect& initial_bounds);
  ~AshWindowTreeHostWin() override;

 private:
  // AshWindowTreeHost:
  void ToggleFullScreen() override;

  // WindowTreeHost:
  void SetBounds(const gfx::Rect& bounds) override;

  bool fullscreen_;
  RECT saved_window_rect_;
  DWORD saved_window_style_;
  DWORD saved_window_ex_style_;

  DISALLOW_COPY_AND_ASSIGN(AshWindowTreeHostWin);
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_WIN_H_
