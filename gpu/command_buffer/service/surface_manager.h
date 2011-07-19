// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SURFACE_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SURFACE_MANAGER_H_

#include "base/basictypes.h"

namespace gfx {
class GLSurface;
}

namespace gpu {

// Interface used to get the GLSurface corresponding to an ID communicated
// through the command buffer.
class SurfaceManager {
 public:
  SurfaceManager();
  virtual ~SurfaceManager();

  virtual gfx::GLSurface* LookupSurface(int id) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SurfaceManager);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SURFACE_MANAGER_H_
