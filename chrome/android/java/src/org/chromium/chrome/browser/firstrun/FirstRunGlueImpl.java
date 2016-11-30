// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Fragment;
import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.signin.AccountAdder;
import org.chromium.components.signin.AccountManagerHelper;

import java.util.List;

/**
 * Provides preferences glue for FirstRunActivity.
 */
public class FirstRunGlueImpl implements FirstRunGlue {
    private static final String CACHED_TOS_ACCEPTED_PREF = "first_run_tos_accepted";

    /**
     * Synchronizes first run native and Java preferences. Previous versions would
     * set only the native pref so this function synchronizes that state to Java.
     * Must be called after native initialization.
     */
    public static void cacheFirstRunPrefs() {
        SharedPreferences javaPrefs = ContextUtils.getAppSharedPreferences();
        if (!javaPrefs.getBoolean(CACHED_TOS_ACCEPTED_PREF, false)
                && PrefServiceBridge.getInstance().isFirstRunEulaAccepted()) {
            javaPrefs.edit().putBoolean(CACHED_TOS_ACCEPTED_PREF, true).apply();
        }
    }

    @Override
    public boolean didAcceptTermsOfService(Context appContext) {
        // Note: Does not check PrefServiceBridge.getInstance().isFirstRunEulaAccepted()
        // because this may be called before native is initialized.
        return ContextUtils.getAppSharedPreferences().getBoolean(CACHED_TOS_ACCEPTED_PREF, false)
                || ToSAckedReceiver.checkAnyUserHasSeenToS(appContext);
    }

    @Override
    public void acceptTermsOfService(boolean allowCrashUpload) {
        UmaSessionStats.changeMetricsReportingConsent(allowCrashUpload);
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(CACHED_TOS_ACCEPTED_PREF, true)
                .apply();
        PrefServiceBridge.getInstance().setEulaAccepted();
    }

    @Override
    public boolean isDefaultAccountName(Context appContext, String accountName) {
        List<String> accountNames = AccountManagerHelper.get(appContext).getGoogleAccountNames();
        return accountNames != null
                && accountNames.size() > 0
                && TextUtils.equals(accountNames.get(0), accountName);
    }

    @Override
    public int numberOfAccounts(Context appContext) {
        List<String> accountNames = AccountManagerHelper.get(appContext).getGoogleAccountNames();
        return accountNames == null ? 0 : accountNames.size();
    }

    @Override
    public void openAccountAdder(Fragment fragment) {
        AccountAdder.getInstance().addAccount(fragment, AccountAdder.ADD_ACCOUNT_RESULT);
    }
}
