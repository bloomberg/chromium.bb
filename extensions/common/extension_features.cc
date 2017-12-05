// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_features.h"

namespace extensions {
namespace features {

// Enables the use of C++-based extension bindings (instead of JS generation).
const base::Feature kNativeCrxBindings{"NativeCrxBindings",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables splitting content script injections into multiple tasks.
const base::Feature kYieldBetweenContentScriptRuns{
    "YieldBetweenContentScriptRuns", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace extensions
