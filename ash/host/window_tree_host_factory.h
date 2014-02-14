// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_WINDOW_TREE_HOST_FACTORY_H_
#define ASH_HOST_WINDOW_TREE_HOST_FACTORY_H_

#include "ash/ash_export.h"
#include "ui/gfx/rect.h"

namespace aura {
class WindowTreeHost;
}

namespace ash {

class ASH_EXPORT WindowTreeHostFactory {
 public:
  virtual ~WindowTreeHostFactory() {}

  static WindowTreeHostFactory* Create();

  // Creates a new aura::WindowTreeHost. The caller owns the returned value.
  virtual aura::WindowTreeHost* CreateWindowTreeHost(
      const gfx::Rect& initial_bounds) = 0;

 protected:
  WindowTreeHostFactory() {}
};

}  // namespace ash

#endif  // ASH_HOST_WINDOW_TREE_HOST_FACTORY_H_
