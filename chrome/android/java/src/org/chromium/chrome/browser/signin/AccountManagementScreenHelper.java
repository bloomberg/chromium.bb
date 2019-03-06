// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.GAIAServiceType;

/**
 * Stub entry points and implementation interface for the account management fragment delegate.
 */
public class AccountManagementScreenHelper {
    @CalledByNative
    private static void openAccountManagementScreen(
            Profile profile, @GAIAServiceType int gaiaServiceType) {
        ThreadUtils.assertOnUiThread();

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            SigninUtils.openSettingsForAllAccounts(ContextUtils.getApplicationContext());
            return;
        }

        AccountManagementFragment.openAccountManagementScreen(gaiaServiceType);
    }

    /**
     * Log a UMA event for a given metric and a signin type.
     * @param metric One of ProfileAccountManagementMetrics constants.
     * @param gaiaServiceType A signin::GAIAServiceType.
     */
    public static void logEvent(int metric, int gaiaServiceType) {
        nativeLogEvent(metric, gaiaServiceType);
    }

    // Native methods.
    private static native void nativeLogEvent(int metric, int gaiaServiceType);
}
