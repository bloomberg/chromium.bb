// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_VIEW_PREFS_H_
#define CHROME_BROWSER_UI_BROWSER_VIEW_PREFS_H_

class PrefServiceSimple;

namespace chrome {

// Register local state preferences specific to BrowserView.
void RegisterBrowserViewPrefs(PrefServiceSimple* prefs);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_VIEW_PREFS_H_
