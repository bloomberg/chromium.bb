// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_WINDOW_RESTORE_UTILS_H_
#define CHROME_BROWSER_UI_COCOA_WINDOW_RESTORE_UTILS_H_

namespace restore_utils {

// On Lion or later, returns |true| if the System Preference
// "Restore windows..." option is checked.  Other OSes will return |false|.
bool IsWindowRestoreEnabled();

}  // namespace restore_utils

#endif  // CHROME_BROWSER_UI_COCOA_WINDOW_RESTORE_UTILS_H_
