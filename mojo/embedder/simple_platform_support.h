// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EMBEDDER_SIMPLE_PLATFORM_SUPPORT_H_
#define MOJO_EMBEDDER_SIMPLE_PLATFORM_SUPPORT_H_

#include "base/macros.h"
#include "mojo/embedder/platform_support.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace embedder {

// A simple implementation of |PlatformSupport|, when sandboxing and
// multiprocess support are not issues (e.g., in most tests). Note: This class
// has no state, and different instances of |SimplePlatformSupport| are mutually
// compatible (i.e., you don't need to use a single instance of it everywhere --
// you may simply create one whenever/wherever you need it).
class MOJO_SYSTEM_IMPL_EXPORT SimplePlatformSupport : public PlatformSupport {
 public:
  SimplePlatformSupport() {}
  virtual ~SimplePlatformSupport() {}

  virtual PlatformSharedBuffer* CreateSharedBuffer(size_t num_bytes) OVERRIDE;
  virtual PlatformSharedBuffer* CreateSharedBufferFromHandle(
      size_t num_bytes,
      ScopedPlatformHandle platform_handle) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SimplePlatformSupport);
};

}  // namespace embedder
}  // namespace mojo

#endif  // MOJO_EMBEDDER_SIMPLE_PLATFORM_SUPPORT_H_
