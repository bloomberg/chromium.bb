// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.provider.Settings;

import org.chromium.base.CalledByNative;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;

/**
 * Stub entry points and implementation interface for the account management fragment delegate.
 */
public class AccountManagementScreenHelper {
    private static AccountManagementFragmentDelegate sDelegate;

    /*
     * TODO(guohui): add all Gaia service types.
     * Enum for the Gaia service types, must match GAIAServiceType in
     * signin_header_helper.h
     */
    public static final int GAIA_SERVICE_TYPE_SIGNUP = 5;

    private static final String EXTRA_ACCOUNT_TYPES = "account_types";
    private static final String EXTRA_VALUE_GOOGLE_ACCOUNTS = "com.google";

    /**
     * An account management screen delegate interface.
     */
    public interface AccountManagementFragmentDelegate {
        /**
         * Opens an incognito tab.
         * @param activity Fragment activity
         */
        void openIncognitoTab(Activity activity);

        /**
         * Opens a sign out dialog.
         * @param activity Fragment activity.
         */
        void openSignOutDialog(Activity activity);

        /**
         * Opens the sync customization preferences fragment.
         * @param activity A preferences activity
         * @param account An account to customize
         */
        void openSyncCustomizationFragment(Preferences activity, Account account);
    }

    /**
     * Sets the account management fragment delegate.
     * @param delegate An implementation of AccountManagementFragmentDelegate interface
     */
    public static void setDelegate(AccountManagementFragmentDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        sDelegate = delegate;
    }

    /**
     * @return An instance of the account management fragment delegate, or null.
     */
    public static AccountManagementFragmentDelegate getDelegate() {
        return sDelegate;
    }

    @CalledByNative
    private static void openAccountManagementScreen(
            Context applicationContext, Profile profile, int gaiaServiceType) {
        ThreadUtils.assertOnUiThread();

        if (gaiaServiceType == GAIA_SERVICE_TYPE_SIGNUP) {
            openAndroidAccountCreationScreen(applicationContext);
            return;
        }

        if (sDelegate == null) return;
        AccountManagementFragment.openAccountManagementScreen(
                applicationContext, profile, gaiaServiceType);
    }

    /**
     * Opens the Android account manager for adding or creating a Google account.
     * @param applicationContext
     */
    private static void openAndroidAccountCreationScreen(
            Context applicationContext) {
        logEvent(ProfileAccountManagementMetrics.DIRECT_ADD_ACCOUNT, GAIA_SERVICE_TYPE_SIGNUP);

        Intent createAccountIntent = new Intent(Settings.ACTION_ADD_ACCOUNT);
        createAccountIntent.putExtra(
                EXTRA_ACCOUNT_TYPES, new String[]{EXTRA_VALUE_GOOGLE_ACCOUNTS});
        createAccountIntent.setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT
                | Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_NEW_TASK
                | Intent.FLAG_ACTIVITY_CLEAR_TOP);

        applicationContext.startActivity(createAccountIntent);
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
