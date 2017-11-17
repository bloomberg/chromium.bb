// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_BASE_FEATURE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_BASE_FEATURE_H_

#include "base/feature_list.h"

// Feature to choose whether the toolbar uses UIViewPropertyAnimators.
extern const base::Feature kPropertyAnimationsToolbar;

// Feature to choose whether to use the clean Toolbar stack or
// WebToolbarController.
extern const base::Feature kCleanToolbar;

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_BASE_FEATURE_H_
