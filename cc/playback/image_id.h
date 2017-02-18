// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_IMAGE_ID_H_
#define CC_PLAYBACK_IMAGE_ID_H_

#include <stdint.h>
#include <unordered_set>

#include "base/containers/flat_set.h"

namespace cc {

using ImageId = uint32_t;
using ImageIdFlatSet = base::flat_set<ImageId>;

}  // namespace cc

#endif  // CC_PLAYBACK_IMAGE_ID_H_
