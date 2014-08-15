// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EMBEDDER_PLATFORM_SUPPORT_H_
#define MOJO_EMBEDDER_PLATFORM_SUPPORT_H_

#include <stddef.h>

#include "base/macros.h"
#include "mojo/embedder/scoped_platform_handle.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace embedder {

class PlatformSharedBuffer;

// This class is provided by the embedder to implement (typically
// platform-dependent) things needed by the Mojo system implementation.
class MOJO_SYSTEM_IMPL_EXPORT PlatformSupport {
 public:
  virtual ~PlatformSupport() {}

  virtual PlatformSharedBuffer* CreateSharedBuffer(size_t num_bytes) = 0;
  virtual PlatformSharedBuffer* CreateSharedBufferFromHandle(
      size_t num_bytes,
      ScopedPlatformHandle platform_handle) = 0;

 protected:
  PlatformSupport() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PlatformSupport);
};

}  // namespace embedder
}  // namespace mojo

#endif  // MOJO_EMBEDDER_PLATFORM_SUPPORT_H_
