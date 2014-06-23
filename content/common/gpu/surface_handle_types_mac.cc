// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/surface_handle_types_mac.h"

#include "base/logging.h"

namespace content {
namespace {

// The type of the handle is stored in the upper 64 bits.
const uint64 kTypeMask = 0xFFFFFFFFull << 32;

const uint64 kTypeIOSurface = 0x01010101ull << 32;
const uint64 kTypeCAContext = 0x02020202ull << 32;

// To make it a bit less likely that we'll just cast off the top bits of the
// handle to get the ID, XOR lower bits with a type-specific mask.
const uint32 kXORMaskIOSurface = 0x01010101;
const uint32 kXORMaskCAContext = 0x02020202;

}  // namespace

SurfaceHandleType GetSurfaceHandleType(uint64 surface_handle) {
  switch(surface_handle & kTypeMask) {
    case kTypeIOSurface:
      return kSurfaceHandleTypeIOSurface;
    case kTypeCAContext:
      return kSurfaceHandleTypeCAContext;
  }
  return kSurfaceHandleTypeInvalid;
}

IOSurfaceID IOSurfaceIDFromSurfaceHandle(uint64 surface_handle) {
  DCHECK_EQ(kSurfaceHandleTypeIOSurface, GetSurfaceHandleType(surface_handle));
  return static_cast<uint32>(surface_handle) ^ kXORMaskIOSurface;
}

CAContextID CAContextIDFromSurfaceHandle(uint64 surface_handle) {
  DCHECK_EQ(kSurfaceHandleTypeCAContext, GetSurfaceHandleType(surface_handle));
  return static_cast<uint32>(surface_handle) ^ kXORMaskCAContext;
}

uint64 SurfaceHandleFromIOSurfaceID(IOSurfaceID io_surface_id) {
  return kTypeIOSurface | (io_surface_id ^ kXORMaskIOSurface);
}

uint64 SurfaceHandleFromCAContextID(CAContextID ca_context_id) {
  return kTypeCAContext | (ca_context_id ^ kXORMaskCAContext);
}

}  //  namespace content
