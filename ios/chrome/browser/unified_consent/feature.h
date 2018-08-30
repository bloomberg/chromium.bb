// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIFIED_CONSENT_FEATURE_H_
#define IOS_CHROME_BROWSER_UNIFIED_CONSENT_FEATURE_H_

// Returns true if the unified consent feature state is kEnabledNoBump or
// kEnabledWithBump. Note that the bump may not be enabled, even if this returns
// true. To check if the bump is enabled, use
// IsUnifiedConsentFeatureWithBumpEnabled().
bool IsUnifiedConsentFeatureEnabled();

// Returns true if the unified consent feature state is kEnabledWithBump.
bool IsUnifiedConsentFeatureWithBumpEnabled();

#endif  // IOS_CHROME_BROWSER_UNIFIED_CONSENT_FEATURE_H_
