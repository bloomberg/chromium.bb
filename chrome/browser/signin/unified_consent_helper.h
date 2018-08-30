// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_UNIFIED_CONSENT_HELPER_H_
#define CHROME_BROWSER_SIGNIN_UNIFIED_CONSENT_HELPER_H_

// TODO(msarda): Move this file in chrome/browser/unified_consent/feature.h

class Profile;

// Returns true if the unified consent feature state is kEnabledNoBump or
// kEnabledWithBump. Note that the bump may not be enabled, even if this returns
// true. To check if the bump is enabled, use
// IsUnifiedConsentFeatureWithBumpEnabled().
bool IsUnifiedConsentFeatureEnabled(Profile* profile);

// Returns true if the unified consent feature state is kEnabledWithBump.
bool IsUnifiedConsentFeatureWithBumpEnabled(Profile* profile);

#endif  // CHROME_BROWSER_SIGNIN_UNIFIED_CONSENT_HELPER_H_
