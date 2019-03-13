// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.app.Activity;
import android.content.Intent;
import android.support.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.ui.base.WindowAndroid;

/**
 * Stub entry points and implementation interface for the account management fragment delegate.
 */
public class AccountManagementScreenHelper {
    @CalledByNative
    private static void openAccountManagementScreen(WindowAndroid windowAndroid,
            @GAIAServiceType int gaiaServiceType, @Nullable String email) {
        ThreadUtils.assertOnUiThread();

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            // If Mice is enabled, directly use the system account management flows.
            switch (gaiaServiceType) {
                case GAIAServiceType.GAIA_SERVICE_TYPE_SIGNUP:
                case GAIAServiceType.GAIA_SERVICE_TYPE_ADDSESSION:
                    AccountManagerFacade accountManagerFacade = AccountManagerFacade.get();
                    @Nullable
                    Account account =
                            email == null ? null : accountManagerFacade.getAccountFromName(email);
                    if (account == null) {
                        // Empty or unknown account: add a new account.
                        // TODO(bsazonov): if email is not empty, pre-fill the account name.
                        startAddAccountActivity(windowAndroid, gaiaServiceType);
                    } else {
                        // Existing account indicates authentication error. Fix it.
                        accountManagerFacade.updateCredentials(
                                account, windowAndroid.getActivity().get(), null);
                    }
                    break;
                default:
                    // Open generic accounts settings.
                    SigninUtils.openSettingsForAllAccounts(ContextUtils.getApplicationContext());
                    break;
            }
            return;
        }

        // If Mice is not enabled, open Chrome's account management screen.
        AccountManagementFragment.openAccountManagementScreen(gaiaServiceType);
    }

    /**
     * Tries starting an Activity to add a Google account to the device. If this activity cannot
     * be started, opens "Accounts" page in the Android Settings app.
     */
    private static void startAddAccountActivity(
            WindowAndroid windowAndroid, @GAIAServiceType int gaiaServiceTypeSignup) {
        logEvent(ProfileAccountManagementMetrics.DIRECT_ADD_ACCOUNT, gaiaServiceTypeSignup);

        AccountManagerFacade.get().createAddAccountIntent((@Nullable Intent intent) -> {
            Activity activity = windowAndroid.getActivity().get();
            if (intent == null || activity == null
                    || !IntentUtils.safeStartActivity(activity, intent)) {
                // Failed to create or show an intent, open settings for all accounts so
                // the user has a chance to create an account manually.
                SigninUtils.openSettingsForAllAccounts(ContextUtils.getApplicationContext());
            }
        });
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
