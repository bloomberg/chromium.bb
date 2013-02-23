// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ID_PROVIDER
#define CC_ANIMATION_ID_PROVIDER

#include "cc/cc_export.h"

namespace cc {

class CC_EXPORT AnimationIdProvider {
 public:
  // These functions each return monotonically increasing values.
  static int NextAnimationId();
  static int NextGroupId();
};

}

#endif  // CC_ANIMATION_ID_PROVIDER
