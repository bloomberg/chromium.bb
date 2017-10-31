// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_MODAL_NTP_H_
#define IOS_CHROME_BROWSER_UI_NTP_MODAL_NTP_H_

#include <Foundation/Foundation.h>

// Whether the panel on the NTP (Bookmarks and Recent Tabs) should be presented
// modally.
BOOL PresentNTPPanelModally();

// Whether chrome://bookmarks is enabled.  True when panel on the NTP isn't
// presented modally.
BOOL IsBookmarksHostEnabled();

#endif  // IOS_CHROME_BROWSER_UI_NTP_MODAL_NTP_H_
