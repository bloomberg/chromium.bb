// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_UTILS_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_UTILS_H_

class TabStripModel;

namespace chrome {

// Returns the index of the first tab that is blocked. This returns
// |model->count()| if no tab is blocked.
int IndexOfFirstBlockedTab(const TabStripModel* model);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_UTILS_H_
