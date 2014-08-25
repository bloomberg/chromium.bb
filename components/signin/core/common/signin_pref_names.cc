// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/common/signin_pref_names.h"

namespace prefs {

// An integer property indicating the state of account id migration from
// email to gaia id for the the profile.  See account_tracker_service.h
// for possible values.
const char kAccountIdMigrationState[] = "account_id_migration_state";

// Boolean identifying whether reverse auto-login is enabled.
const char kAutologinEnabled[] = "autologin.enabled";

// String the identifies the last user that logged into sync and other
// google services. As opposed to kGoogleServicesUsername, this value is not
// cleared on signout, but while the user is signed in the two values will
// be the same.
const char kGoogleServicesLastUsername[] = "google.services.last_username";

// Obfuscated account ID that identifies the current user logged into sync and
// other google services.
const char kGoogleServicesUserAccountId[] = "google.services.user_account_id";

// String that identifies the current user logged into sync and other google
// services.
const char kGoogleServicesUsername[] = "google.services.username";

// Device id scoped to single signin. This device id will be regenerated if user
// signs out and signs back in. When refresh token is requested for this user it
// will be annotated with this device id.
const char kGoogleServicesSigninScopedDeviceId[] =
    "google.services.signin_scoped_device_id";

// Local state pref containing a string regex that restricts which accounts
// can be used to log in to chrome (e.g. "*@google.com"). If missing or blank,
// all accounts are allowed (no restrictions).
const char kGoogleServicesUsernamePattern[] =
    "google.services.username_pattern";

// Boolean identifying whether reverse auto-logins is enabled.
const char kReverseAutologinEnabled[] = "reverse_autologin.enabled";

// List to keep track of emails for which the user has rejected one-click
// sign-in.
const char kReverseAutologinRejectedEmailList[] =
    "reverse_autologin.rejected_email_list";

// Int64 which tracks, as time from epoch, when last time the user signed in
// to the browser.
const char kSignedInTime[] = "signin.signedin_time";

// Boolean which stores if the user is allowed to signin to chrome.
const char kSigninAllowed[] = "signin.allowed";

}  // namespace prefs
