// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NEW_TAB_PAGE_URL_HANDLER_H_
#define CHROME_BROWSER_ANDROID_NEW_TAB_PAGE_URL_HANDLER_H_

class GURL;

namespace content {
class BrowserContext;
}

namespace chrome {
namespace android {

// Rewrites old-style Android NTP URLs to new-style NTP URLs:
//  - chrome://newtab              -> chrome-native://newtab
//  - chrome://newtab#most_visited -> chrome-native://newtab
//  - chrome://newtab#incognito    -> chrome-native://newtab
//  - chrome://newtab#bookmarks    -> chrome-native://bookmarks
//  - chrome://newtab#bookmarks:99 -> chrome-native://bookmarks
//  - chrome://newtab#open_tabs    -> chrome-native://recent-tabs
//  - chrome-native://recent_tabs  -> chrome-native://recent-tabs
//
// TODO(newt): Once most users have upgraded past M34, simplify this down to a
// single rule: chrome://newtab -> chrome-native://newtab
bool HandleAndroidNewTabURL(GURL* url,
                            content::BrowserContext* browser_context);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_NEW_TAB_PAGE_URL_HANDLER_H_
