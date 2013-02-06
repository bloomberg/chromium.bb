// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_LAYOUT_TYPE_PREFS_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_LAYOUT_TYPE_PREFS_H_

class PrefRegistrySimple;

namespace chrome {

// Register local state preferences specific to the TabStrip.
void RegisterTabStripLayoutTypePrefs(PrefRegistrySimple* registry);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_LAYOUT_TYPE_PREFS_H_
