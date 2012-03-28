// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "../client/share_group.h"
#include "../common/logging.h"

namespace gpu {
namespace gles2 {

ShareGroup::ShareGroup() {
  GPU_CHECK(ShareGroup::ImplementsThreadSafeReferenceCounting());
}

ShareGroup::~ShareGroup() {
}

}  // namespace gles2
}  // namespace gpu

