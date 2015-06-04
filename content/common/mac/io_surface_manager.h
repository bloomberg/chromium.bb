// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MAC_IO_SURFACE_MANAGER_H_
#define CONTENT_COMMON_MAC_IO_SURFACE_MANAGER_H_

#include <IOSurface/IOSurfaceAPI.h>

#include "content/common/content_export.h"

namespace content {

// This interface provides a mechanism for different processes to securely
// share IOSurfaces. A process can register an IOSurface for use in another
// process and the client ID provided with registration determines the process
// that can acquire a reference to the IOSurface.
class CONTENT_EXPORT IOSurfaceManager {
 public:
  static IOSurfaceManager* GetInstance();
  static void SetInstance(IOSurfaceManager* instance);

  // Register an IO surface for use in another process.
  virtual bool RegisterIOSurface(int io_surface_id,
                                 int client_id,
                                 IOSurfaceRef io_surface) = 0;

  // Unregister an IO surface previously registered for use in another
  // process.
  virtual void UnregisterIOSurface(int io_surface_id, int client_id) = 0;

  // Acquire IO surface reference for a registered IO surface.
  virtual IOSurfaceRef AcquireIOSurface(int io_surface_id) = 0;

 protected:
  virtual ~IOSurfaceManager() {}
};

}  // namespace content

#endif  // CONTENT_COMMON_MAC_IO_SURFACE_MANAGER_H_
