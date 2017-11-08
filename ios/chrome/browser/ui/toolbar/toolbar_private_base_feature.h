// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_PRIVATE_BASE_FEATURE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_PRIVATE_BASE_FEATURE_H_

#include "base/feature_list.h"

// Feature to choose whether the toolbar respects the safe area.
// Do not use it directly, uses the IsSafeAreaCompatibleToolbarEnabled()
// function instead.
extern const base::Feature kSafeAreaCompatibleToolbar;

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_PRIVATE_BASE_FEATURE_H_
