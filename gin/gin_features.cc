// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/gin_features.h"

namespace features {

// Enables or disables the future compiler pipeline of V8, with the Ignition
// interpreter and TurboFan compiler.
const base::Feature kV8Future {
  "V8Future", base::FEATURE_DISABLED_BY_DEFAULT
};

// Enables or disables the V8 Ignition interpreter on low end
// Android devices.
const base::Feature kV8IgnitionLowEnd {
  "V8IgnitionLowEnd", base::FEATURE_ENABLED_BY_DEFAULT
};

}  // namespace features
