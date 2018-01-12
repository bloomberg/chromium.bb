// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_PREF_NAMES_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_PREF_NAMES_H_

namespace prefs {

#if defined(OS_CHROMEOS)
extern const char kAccountConsistencyMirrorRequired[];
#endif
extern const char kAccountIdMigrationState[];
extern const char kAutologinEnabled[];
extern const char kGaiaCookieHash[];
extern const char kGaiaCookieChangedTime[];
extern const char kGaiaCookiePeriodicReportTime[];
extern const char kGoogleServicesAccountId[];
extern const char kGoogleServicesHostedDomain[];
extern const char kGoogleServicesLastAccountId[];
extern const char kGoogleServicesLastUsername[];
extern const char kGoogleServicesSigninScopedDeviceId[];
extern const char kGoogleServicesUserAccountId[];
extern const char kGoogleServicesUsername[];
extern const char kGoogleServicesUsernamePattern[];
extern const char kReverseAutologinEnabled[];
extern const char kReverseAutologinRejectedEmailList[];
extern const char kSignedInTime[];
extern const char kSigninAllowed[];
extern const char kTokenServiceExcludeAllSecondaryAccounts[];
extern const char kTokenServiceExcludedSecondaryAccounts[];

}  // namespace prefs

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_PREF_NAMES_H_
