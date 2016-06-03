// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BACKGROUND_LAYOUT_H_
#define ASH_MUS_BACKGROUND_LAYOUT_H_

#include "ash/mus/layout_manager.h"
#include "base/macros.h"

namespace ash {
namespace mus {

// Lays out the desktop background within background containers.
class BackgroundLayout : public LayoutManager {
 public:
  explicit BackgroundLayout(::mus::Window* owner);
  ~BackgroundLayout() override;

 private:
  // Overridden from LayoutManager:
  void LayoutWindow(::mus::Window* window) override;

  DISALLOW_COPY_AND_ASSIGN(BackgroundLayout);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BACKGROUND_LAYOUT_H_
