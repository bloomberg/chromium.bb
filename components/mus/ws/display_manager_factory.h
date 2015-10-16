// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_DISPLAY_MANAGER_FACTORY_H_
#define COMPONENTS_MUS_WS_DISPLAY_MANAGER_FACTORY_H_

#include "components/mus/gles2/gpu_state.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/callback.h"

namespace mus {
class ApplicationImpl;
}  // namespace mus

namespace mus {

class DisplayManager;

// Abstract factory for DisplayManagers. Used by tests to construct test
// DisplayManagers.
class DisplayManagerFactory {
 public:
  virtual DisplayManager* CreateDisplayManager(
      mojo::ApplicationImpl* app_impl,
      const scoped_refptr<GpuState>& gpu_state,
      const scoped_refptr<SurfacesState>& surfaces_state) = 0;
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_DISPLAY_MANAGER_FACTORY_H_
