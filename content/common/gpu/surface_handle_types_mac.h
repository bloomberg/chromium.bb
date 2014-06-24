// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_SURFACE_HANDLE_TYPES_MAC_H_
#define CONTENT_COMMON_GPU_SURFACE_HANDLE_TYPES_MAC_H_

#include <IOSurface/IOSurface.h>
#include <OpenGL/CGLIOSurface.h>

#include "base/basictypes.h"
#include "ui/base/cocoa/remote_layer_api.h"

namespace content {

// The surface handle passed between the GPU and browser process may refer to
// an IOSurface or a CAContext. These helper functions must be used to identify
// and translate between the types.
enum SurfaceHandleType {
  kSurfaceHandleTypeInvalid,
  kSurfaceHandleTypeIOSurface,
  kSurfaceHandleTypeCAContext,
};

SurfaceHandleType GetSurfaceHandleType(uint64 surface_handle);

CAContextID CAContextIDFromSurfaceHandle(uint64 surface_handle);
IOSurfaceID IOSurfaceIDFromSurfaceHandle(uint64 surface_handle);

uint64 SurfaceHandleFromIOSurfaceID(IOSurfaceID io_surface_id);
uint64 SurfaceHandleFromCAContextID(CAContextID ca_context_id);

}  //  namespace content

#endif // CONTENT_COMMON_GPU_HANDLE_TYPES_MAC_H_
