// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_SCREENLOCK_LAYOUT_H_
#define ASH_MUS_SCREENLOCK_LAYOUT_H_

#include "ash/mus/layout_manager.h"
#include "base/macros.h"

namespace ash {
namespace mus {

// Lays out the shelf within shelf containers.
class ScreenlockLayout : public LayoutManager {
 public:
  explicit ScreenlockLayout(::ui::Window* owner);
  ~ScreenlockLayout() override;

 private:
  // Overridden from LayoutManager:
  void LayoutWindow(::ui::Window* window) override;

  DISALLOW_COPY_AND_ASSIGN(ScreenlockLayout);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_SCREENLOCK_LAYOUT_H_
