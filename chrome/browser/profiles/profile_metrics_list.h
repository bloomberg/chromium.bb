// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Intentionally no include guards because this file is meant to be included
// inside a macro to generate enum values.

// Enum for tracking user interactions with the account management menu
// on Android.

// User arrived at the Account management screen.
PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU(VIEW, 0)
// User arrived at the Account management screen, and clicked Add account.
PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU(ADD_ACCOUNT, 1)
// User arrived at the Account management screen, and clicked Go incognito.
PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU(GO_INCOGNITO, 2)
// User arrived at the Account management screen, and clicked on primary.
PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU(CLICK_PRIMARY_ACCOUNT, 3)
// User arrived at the Account management screen, and clicked on secondary.
PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU(CLICK_SECONDARY_ACCOUNT, 4)
// User arrived at the Account management screen, toggled Chrome signout.
PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU(TOGGLE_SIGNOUT, 5)
// User toggled Chrome signout, and clicked Signout.
PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU(SIGNOUT_SIGNOUT, 6)
// User toggled Chrome signout, and clicked Cancel.
PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU(SIGNOUT_CANCEL, 7)
// User arrived at the android Account management screen directly from some
// Gaia requests.
PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU(DIRECT_ADD_ACCOUNT, 8)

