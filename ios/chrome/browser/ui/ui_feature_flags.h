// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
#define IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_

#include "base/feature_list.h"

// Used to enable the first phase of the UI refresh. This flag should not be
// used directly. Instead use ui_util::IsUIRefreshPhase1Enabled().
extern const base::Feature kUIRefreshPhase1;

// Used to enable the tab grid on phone and tablet. This flag should not be
// used directly. Instead use ui_util::IsTabSwitcherTabGridEnabled().
extern const base::Feature kTabSwitcherTabGrid;

#endif  // IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
