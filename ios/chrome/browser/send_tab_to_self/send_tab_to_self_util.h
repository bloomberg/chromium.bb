// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_

class GURL;

namespace ios {
class ChromeBrowserState;
}

namespace send_tab_to_self {

// Returns true if the 'send tab to self' flag is enabled.
bool IsReceivingEnabled();

// Returns true if the 'send-tab-to-self' and 'send-tab-to-self-show-sending-ui'
// flags are enabled.
bool IsSendingEnabled();

// Returns true if the SendTabToSelf sync datatype is active.
bool IsUserSyncTypeActive(ios::ChromeBrowserState* browser_state);

// Returns true if the user syncing on two or more devices.
bool IsSyncingOnMultipleDevices(ios::ChromeBrowserState* browser_state);

// Returns true if the tab and web content requirements are met:
//  User is viewing an HTTP or HTTPS page.
//  User is not on a native page.
//  User is not in Incongnito mode.
bool IsContentRequirementsMet(const GURL& gurl,
                              ios::ChromeBrowserState* browser_state);

// Returns true if all conditions are true and shows the option onto the menu.
bool ShouldOfferFeature();

// Add a new entry to SendTabToSelfModel when user click "Share to your
// devices" option.
void CreateNewEntry(ios::ChromeBrowserState* browser_state);

}  // namespace send_tab_to_self

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
