// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Intent;
import android.provider.Settings;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.components.signin.GAIAServiceType;

/**
 * Stub entry points and implementation interface for the account management fragment delegate.
 */
public class AccountManagementScreenHelper {
    private static final String EXTRA_ACCOUNT_TYPES = "account_types";
    private static final String EXTRA_VALUE_GOOGLE_ACCOUNTS = "com.google";

    @CalledByNative
    private static void openAccountManagementScreen(
            Profile profile, @GAIAServiceType int gaiaServiceType) {
        ThreadUtils.assertOnUiThread();

        if (gaiaServiceType == GAIAServiceType.GAIA_SERVICE_TYPE_SIGNUP) {
            openAndroidAccountCreationScreen();
            return;
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            SigninUtils.openSettingsForAllAccounts(ContextUtils.getApplicationContext());
            return;
        }

        AccountManagementFragment.openAccountManagementScreen(gaiaServiceType);
    }

    /**
     * Opens the Android account manager for adding or creating a Google account.
     */
    private static void openAndroidAccountCreationScreen() {
        logEvent(ProfileAccountManagementMetrics.DIRECT_ADD_ACCOUNT,
                GAIAServiceType.GAIA_SERVICE_TYPE_SIGNUP);

        Intent createAccountIntent = new Intent(Settings.ACTION_ADD_ACCOUNT);
        createAccountIntent.putExtra(
                EXTRA_ACCOUNT_TYPES, new String[]{EXTRA_VALUE_GOOGLE_ACCOUNTS});
        createAccountIntent.setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT
                | Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_NEW_TASK
                | Intent.FLAG_ACTIVITY_CLEAR_TOP);

        ContextUtils.getApplicationContext().startActivity(createAccountIntent);
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
