// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_features.h"

namespace content {

#if defined(OS_ANDROID)
const base::Feature kSeccompSandboxAndroidFeature = {
  "SeccompSandboxAndroid", base::FEATURE_DISABLED_BY_DEFAULT
};
#endif

}  // namespace content
