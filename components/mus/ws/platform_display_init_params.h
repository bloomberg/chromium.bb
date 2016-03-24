// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_PLATFORM_DISPLAY_INIT_PARAMS_H_
#define COMPONENTS_MUS_WS_PLATFORM_DISPLAY_INIT_PARAMS_H_

#include "base/memory/ref_counted.h"

namespace mojo {
class Connector;
}

namespace mus {

class GpuState;
class SurfacesState;

namespace ws {

struct PlatformDisplayInitParams {
  PlatformDisplayInitParams();
  PlatformDisplayInitParams(const PlatformDisplayInitParams& other);
  ~PlatformDisplayInitParams();

  mojo::Connector* connector = nullptr;
  scoped_refptr<GpuState> gpu_state;
  scoped_refptr<SurfacesState> surfaces_state;
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_PLATFORM_DISPLAY_INIT_PARAMS_H_
