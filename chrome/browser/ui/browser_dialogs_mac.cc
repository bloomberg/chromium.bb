// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_dialogs.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "chrome/common/chrome_switches.h"

const base::Feature kMacViewsWebUIDialogs {
  "MacViewsWebUIDialogs", base::FEATURE_DISABLED_BY_DEFAULT
};

namespace chrome {

bool ToolkitViewsDialogsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableMacViewsDialogs);
}

bool ToolkitViewsWebUIDialogsEnabled() {
  return base::FeatureList::IsEnabled(kMacViewsWebUIDialogs);
}

}  // namespace chrome
