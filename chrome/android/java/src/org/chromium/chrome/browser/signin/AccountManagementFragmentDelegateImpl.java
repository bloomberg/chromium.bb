// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.signin.AccountManagementScreenHelper.AccountManagementFragmentDelegate;
import org.chromium.chrome.browser.sync.ui.SyncCustomizationFragment;

/**
 * Implementation of the account management fragment delegate.
 * TODO(aruslan): delete this class once all of its dependencies are upstream.
 */
public class AccountManagementFragmentDelegateImpl implements AccountManagementFragmentDelegate {
    /**
     * Registers the account management fragment delegate implementation.
     */
    public static void registerAccountManagementFragmentDelegate() {
        AccountManagementScreenHelper.setDelegate(new AccountManagementFragmentDelegateImpl());
    }

    @Override
    public void openIncognitoTab(Activity activity) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(
                IntentHandler.GOOGLECHROME_NAVIGATE_PREFIX + UrlConstants.NTP_URL));
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        intent.setPackage(activity.getApplicationContext().getPackageName());
        intent.setClassName(activity.getApplicationContext().getPackageName(),
                ChromeLauncherActivity.class.getName());

        intent.setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT
                | Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_NEW_TASK
                | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        IntentHandler.startActivityForTrustedIntent(intent, activity);
    }

    @Override
    public void openSignOutDialog(Activity activity) {
        SigninManager.get(activity).signOut(activity, null);
    }

    @Override
    public void openSyncCustomizationFragment(final Preferences activity, final Account account) {
        Bundle args = new Bundle();
        args.putString(SyncCustomizationFragment.ARGUMENT_ACCOUNT, account.name);
        activity.startFragment(SyncCustomizationFragment.class.getName(), args);
    }
}
