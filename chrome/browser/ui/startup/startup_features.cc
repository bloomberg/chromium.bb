// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_features.h"

namespace features {

const base::Feature kUseConsolidatedStartupFlow{
    "UseConsolidatedStartupFlow", base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_WIN)
const base::Feature kEnableWelcomeWin10{"EnableWelcomeWin10",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kWelcomeWin10InlineStyle{"WelcomeWin10InlineStyle",
                                             base::FEATURE_ENABLED_BY_DEFAULT};
#endif

}  // namespace features
