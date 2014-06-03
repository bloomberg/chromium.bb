// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;

import org.chromium.base.CalledByNative;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Stub entry points and implementation interface for the account management screen.
 */
public class AccountManagementScreenHelper {
    private static AccountManagementScreenManager sManager;

    /*
     * TODO(aruslan): http://crbug.com/379987 Move to a generator.
     * Enum for tracking user interactions with the account management menu
     * on Android.
     * This must match enum ProfileAndroidAccountManagementMenu in profile_metrics.h.
     */
    // User arrived at the Account management screen.
    public static final int ACCOUNT_MANAGEMENT_MENU_VIEW = 0;
    // User arrived at the Account management screen, and clicked Add account.
    public static final int ACCOUNT_MANAGEMENT_MENU_ADD_ACCOUNT = 1;
    // User arrived at the Account management screen, and clicked Go incognito.
    public static final int ACCOUNT_MANAGEMENT_MENU_GO_INCOGNITO = 2;
    // User arrived at the Account management screen, and clicked on primary.
    public static final int ACCOUNT_MANAGEMENT_MENU_CLICK_PRIMARY_ACCOUNT = 3;
    // User arrived at the Account management screen, and clicked on secondary.
    public static final int ACCOUNT_MANAGEMENT_MENU_CLICK_SECONDARY_ACCOUNT = 4;
    // User arrived at the Account management screen, toggled Chrome signout.
    public static final int ACCOUNT_MANAGEMENT_MENU_TOGGLE_SIGNOUT = 5;
    // User toggled Chrome signout, and clicked Signout.
    public static final int ACCOUNT_MANAGEMENT_MENU_SIGNOUT_SIGNOUT = 6;
    // User toggled Chrome signout, and clicked Cancel.
    public static final int ACCOUNT_MANAGEMENT_MENU_SIGNOUT_CANCEL = 7;

    /**
     * The screen manager interface.
     */
    public interface AccountManagementScreenManager {
        /**
         * Opens the account management UI screen.
         * @param applicationContext The application context.
         * @param profile The user profile.
         * @param gaiaServiceType A signin::GAIAServiceType value that triggered the dialog.
         */
        void openAccountManagementScreen(
                Context applicationContext, Profile profile, int gaiaServiceType);
    }

    /**
     * Sets the screen manager.
     * @param manager An implementation of AccountManagementScreenManager interface.
     */
    public static void setManager(AccountManagementScreenManager manager) {
        ThreadUtils.assertOnUiThread();
        sManager = manager;
    }

    @CalledByNative
    private static void openAccountManagementScreen(
            Context applicationContext, Profile profile, int gaiaServiceType) {
        ThreadUtils.assertOnUiThread();
        if (sManager == null) return;

        sManager.openAccountManagementScreen(applicationContext, profile, gaiaServiceType);
    }

    /**
     * Log a UMA event for a given metric and a signin type.
     * @param metric A PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU metric.
     * @param gaiaServiceType A signin::GAIAServiceType.
     */
    public static void logEvent(int metric, int gaiaServiceType) {
        nativeLogEvent(metric, gaiaServiceType);
    }

    // Native methods.
    private static native void nativeLogEvent(int metric, int gaiaServiceType);
}
