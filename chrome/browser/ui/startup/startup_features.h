// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_FEATURES_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_FEATURES_H_

#include "base/feature_list.h"

namespace features {

extern const base::Feature kUseConsolidatedStartupFlow;

#if defined(OS_WIN)
extern const base::Feature kEnableWelcomeWin10;
extern const base::Feature kWelcomeWin10InlineStyle;
#endif

}  // namespace features

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_FEATURES_H_
