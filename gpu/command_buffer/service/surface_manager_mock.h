// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SURFACE_MANAGER_MOCK_H_
#define GPU_COMMAND_BUFFER_SERVICE_SURFACE_MANAGER_MOCK_H_

#include "gpu/command_buffer/service/surface_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu {

class MockSurfaceManager : public SurfaceManager {
 public:
  MockSurfaceManager();
  virtual ~MockSurfaceManager();

  MOCK_METHOD1(LookupSurface, gfx::GLSurface*(int id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSurfaceManager);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SURFACE_MANAGER_MOCK_H_
