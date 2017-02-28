// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_TEST_MESSAGE_LOOP_TYPE_H_
#define GPU_TEST_MESSAGE_LOOP_TYPE_H_

#include "base/message_loop/message_loop.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace gpu {
namespace test {

// Returns the MessageLoop type needed for GPU tests. The Ozone platform may not
// work with TYPE_DEFAULT and this needs to be checked at runtime.
inline base::MessageLoop::Type GetMessageLoopTypeForGpu() {
#if defined(USE_OZONE)
  return ui::OzonePlatform::EnsureInstance()->GetMessageLoopTypeForGpu();
#else
  return base::MessageLoop::TYPE_DEFAULT;
#endif
}

}  // namespace test
}  // namespace gpu

#endif  // GPU_TEST_MESSAGE_LOOP_TYPE_H_
