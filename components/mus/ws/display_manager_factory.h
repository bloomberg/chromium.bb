// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_DISPLAY_MANAGER_FACTORY_H_
#define COMPONENTS_MUS_WS_DISPLAY_MANAGER_FACTORY_H_

#include "components/mus/gles2/gpu_state.h"
#include "mojo/public/cpp/bindings/callback.h"

namespace mojo {
class Shell;
}

namespace mus {

namespace ws {

class DisplayManager;

// Abstract factory for DisplayManagers. Used by tests to construct test
// DisplayManagers.
class DisplayManagerFactory {
 public:
  virtual DisplayManager* CreateDisplayManager(
      mojo::Shell* shell,
      const scoped_refptr<mus::GpuState>& gpu_state,
      const scoped_refptr<mus::SurfacesState>& surfaces_state) = 0;
};

}  // namespace ws

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_DISPLAY_MANAGER_FACTORY_H_
