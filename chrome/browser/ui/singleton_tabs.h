// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SINGLETON_TABS_H_
#define CHROME_BROWSER_UI_SINGLETON_TABS_H_

class Browser;
class GURL;

// Methods for opening "singleton tabs". Tabs are guaranteed unique by varying
// metrics within a particular Browser window.

namespace chrome {

struct NavigateParams;

// Core singleton tab API:

// Show a given a URL. If a tab with the same URL (ignoring the ref) is
// already visible in this browser, it becomes selected. Otherwise a new tab
// is created.
void ShowSingletonTab(Browser* browser, const GURL& url);

// Same as ShowSingletonTab, but does not ignore ref.
void ShowSingletonTabRespectRef(Browser* browser, const GURL& url);

// As ShowSingletonTab, but if the current tab is the new tab page or
// about:blank, then overwrite it with the passed contents.
void ShowSingletonTabOverwritingNTP(Browser* browser,
                                    const NavigateParams& params);

// Creates a NavigateParams struct for a singleton tab navigation.
NavigateParams GetSingletonTabNavigateParams(Browser* browser, const GURL& url);

// If the given navigational URL is a Singleton, return the tab index for it.
// Otherwise, returns -1.
int GetIndexOfSingletonTab(NavigateParams* params);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SINGLETON_TABS_H_
