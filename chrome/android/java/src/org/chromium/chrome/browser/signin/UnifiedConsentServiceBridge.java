// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import org.chromium.chrome.browser.profiles.Profile;

/**
 * Bridge to UnifiedConsentService.
 */
public class UnifiedConsentServiceBridge {
    private UnifiedConsentServiceBridge() {}

    /**
     * Returns whether the consent bump should be shown as part of the migration to Unified Consent.
     */
    public static boolean shouldShowConsentBump() {
        return nativeShouldShowConsentBump(Profile.getLastUsedProfile());
    }

    private static native boolean nativeShouldShowConsentBump(Profile profile);
}
