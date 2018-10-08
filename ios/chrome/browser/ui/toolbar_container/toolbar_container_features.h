// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_TOOLBAR_CONTAINER_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_TOOLBAR_CONTAINER_FEATURES_H_

#include "base/feature_list.h"

namespace toolbar_container {

// Used to enable the fix for crbug.com/889884.  This is a temporary flag that
// is only going to be used as an emergency shutoff for the bug fix because it
// was landed late in the branch.
// TODO(crbug.com/880672): Remove this flag.
extern const base::Feature kToolbarContainerCustomViewEnabled;

// Used to move toolbar layout management to a container view.
extern const base::Feature kToolbarContainerEnabled;

}  // namespace toolbar_container

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_TOOLBAR_CONTAINER_FEATURES_H_
