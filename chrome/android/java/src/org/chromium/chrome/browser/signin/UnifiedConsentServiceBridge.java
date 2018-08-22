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

    /** Sets whether the user has given unified consent. */
    public static void setUnifiedConsentGiven(boolean unifiedConsentGiven) {
        nativeSetUnifiedConsentGiven(Profile.getLastUsedProfile(), unifiedConsentGiven);
    }

    /** Returns whether the user has given unified consent. */
    public static boolean isUnifiedConsentGiven() {
        return nativeIsUnifiedConsentGiven(Profile.getLastUsedProfile());
    }

    /**
     * Returns whether the consent bump should be shown as part of the migration to Unified Consent.
     */
    public static boolean shouldShowConsentBump() {
        return nativeShouldShowConsentBump(Profile.getLastUsedProfile());
    }

    private static native void nativeSetUnifiedConsentGiven(Profile profile, boolean consentGiven);
    private static native boolean nativeIsUnifiedConsentGiven(Profile lastUsedProfile);

    private static native boolean nativeShouldShowConsentBump(Profile profile);
}
