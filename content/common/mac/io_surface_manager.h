// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MAC_IO_SURFACE_MANAGER_H_
#define CONTENT_COMMON_MAC_IO_SURFACE_MANAGER_H_

#include <IOSurface/IOSurface.h>

#include "content/common/content_export.h"
#include "ui/gfx/generic_shared_memory_id.h"

namespace content {

using IOSurfaceId = gfx::GenericSharedMemoryId;

// This interface provides a mechanism for different processes to securely
// share IOSurfaces. A process can register an IOSurface for use in another
// process and the client ID provided with registration determines the process
// that can acquire a reference to the IOSurface.
class CONTENT_EXPORT IOSurfaceManager {
 public:
  static IOSurfaceManager* GetInstance();
  static void SetInstance(IOSurfaceManager* instance);

  // Register an IO surface for use in another process.
  virtual bool RegisterIOSurface(IOSurfaceId io_surface_id,
                                 int client_id,
                                 IOSurfaceRef io_surface) = 0;

  // Unregister an IO surface previously registered for use in another
  // process.
  virtual void UnregisterIOSurface(IOSurfaceId io_surface_id,
                                   int client_id) = 0;

  // Acquire IO surface reference for a registered IO surface.
  virtual IOSurfaceRef AcquireIOSurface(IOSurfaceId io_surface_id) = 0;

 protected:
  virtual ~IOSurfaceManager() {}
};

}  // namespace content

#endif  // CONTENT_COMMON_MAC_IO_SURFACE_MANAGER_H_
