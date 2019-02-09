// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_

class Browser;
class GURL;
class Profile;

namespace send_tab_to_self {

// The send tab to self util contains a list of helper functions.

// Returns true if the 'send tab to self' flag is enabled.
bool IsFlagEnabled();

// Returns true if the SendTabToSelf sync datatype is enabled.
bool IsUserSyncTypeEnabled(Profile* profile);

// Returns true if the user syncing on two or more devices.
bool IsSyncingOnMultipleDevices(Profile* profile);

// Returns true if the tab and web content requirements are met:
//  User is viewing an HTTP or HTTPS page.
//  User is not on a native page.
//  User is not in Incongnito mode.
bool IsContentRequirementsMet(GURL& gurl, Profile* profile);

// Returns true if all conditions are true and shows the option onto the menu
bool ShouldOfferFeature(Browser* browser);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
