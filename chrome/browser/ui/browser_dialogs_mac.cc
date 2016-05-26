// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_dialogs.h"

#include "base/feature_list.h"

namespace chrome {

const base::Feature kMacViewsNativeDialogs {
  "MacViewsNativeDialogs", base::FEATURE_DISABLED_BY_DEFAULT
};

const base::Feature kMacViewsWebUIDialogs {
  "MacViewsWebUIDialogs", base::FEATURE_DISABLED_BY_DEFAULT
};

bool ToolkitViewsDialogsEnabled() {
  return base::FeatureList::IsEnabled(kMacViewsNativeDialogs);
}

bool ToolkitViewsWebUIDialogsEnabled() {
  return base::FeatureList::IsEnabled(kMacViewsWebUIDialogs);
}

}  // namespace chrome
