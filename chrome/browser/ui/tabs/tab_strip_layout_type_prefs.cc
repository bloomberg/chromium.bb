// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_layout_type_prefs.h"

#include "base/prefs/pref_registry_simple.h"
#include "chrome/browser/ui/tabs/tab_strip_layout_type.h"
#include "chrome/common/pref_names.h"

namespace chrome {

void RegisterTabStripLayoutTypePrefs(PrefRegistrySimple* registry) {
  // This value is device dependant, so it goes in local state.
  registry->RegisterIntegerPref(prefs::kTabStripLayoutType,
                                static_cast<int>(TAB_STRIP_LAYOUT_SHRINK));
}

}  // namespace chrome
