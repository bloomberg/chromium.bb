// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/components/captive_portal/features.h"

namespace captive_portal {

#if defined(OS_IOS)
const base::Feature kIosCaptivePortal{"IosCaptivePortal",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace captive_portal