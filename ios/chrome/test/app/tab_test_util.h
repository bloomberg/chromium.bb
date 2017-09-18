// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_TAB_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_TAB_TEST_UTIL_H_

#import <Foundation/Foundation.h>

@class Tab;

namespace chrome_test_util {

// Opens a new tab, and does not wait for animations to complete.
void OpenNewTab();

// Opens a new incognito tab, and does not wait for animations to complete.
void OpenNewIncognitoTab();

// Returns YES if the browser is in incognito mode, and NO otherwise.
BOOL IsIncognitoMode();

// Gets current tab.
Tab* GetCurrentTab();

// Gets next tab and returns nil if less than two tabs are open.
Tab* GetNextTab();

// Closes current tab.
void CloseCurrentTab();

// Closes tab with the given index in current mode (incognito or normal).
void CloseTabAtIndex(NSUInteger index);

// Closes all tabs in the current mode (incognito or normal), and does not wait
// for the UI to complete. If current mode is Incognito, mode will be switched
// normal after closing all tabs.
void CloseAllTabsInCurrentMode();

// Closes all tabs in the all modes (incognito and normal), and does not wait
// for the UI to complete.
// If current mode is Incognito, mode will be switched to normal after closing
// the incognito tabs.
void CloseAllTabs();

// Selects tab with given index in current mode (incognito or normal).
void SelectTabAtIndexInCurrentMode(NSUInteger index);

// Returns the number of main tabs.
NSUInteger GetMainTabCount();

// Returns the number of incognito tabs.
NSUInteger GetIncognitoTabCount();

// Resets the tab usage recorder on current mode. Return YES on success.
BOOL ResetTabUsageRecorder();

// Sets the normal tabs as 'cold start' tabs. Return YES on success.
BOOL SetCurrentTabsToBeColdStartTabs();

// Simulates a backgrounding. Return YES on success.
BOOL SimulateTabsBackgrounding();

// Evicts the tabs associated with the non-current browser mode.
void EvictOtherTabModelTabs();

// Closes all incognito tabs.
void CloseAllIncognitoTabs();

// Returns the number of main tabs currently evicted.
NSUInteger GetEvictedMainTabCount();

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_TAB_TEST_UTIL_H_
