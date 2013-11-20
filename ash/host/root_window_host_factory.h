// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_ROOT_WINDOW_HOST_FACTORY_H_
#define ASH_HOST_ROOT_WINDOW_HOST_FACTORY_H_

#include "ash/ash_export.h"
#include "ui/gfx/rect.h"

namespace aura {
class RootWindowHost;
}

namespace ash {

class ASH_EXPORT RootWindowHostFactory {
 public:
  virtual ~RootWindowHostFactory() {}

  static RootWindowHostFactory* Create();

  // Creates a new aura::RootWindowHost. The caller owns the returned value.
  virtual aura::RootWindowHost* CreateRootWindowHost(
      const gfx::Rect& initial_bounds) = 0;

 protected:
  RootWindowHostFactory() {}
};

}  // namespace ash

#endif  // ASH_HOST_ROOT_WINDOW_HOST_FACTORY_H_
