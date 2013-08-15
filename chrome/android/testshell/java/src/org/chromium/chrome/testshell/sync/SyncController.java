// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell.sync;

import android.accounts.Account;
import android.app.Activity;
import android.app.FragmentManager;
import android.content.Context;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.sync.notifier.SyncStatusHelper;
import org.chromium.sync.signin.AccountManagerHelper;

/**
 * A helper class for signing in and out of Chromium.
 */
public class SyncController {

    private static SyncController sInstance;

    private final Context mContext;

    private SyncController(Context context) {
        mContext = context;
    }

    /**
     * Retrieve the singleton instance of this class.
     *
     * @param context the current context.
     * @return the singleton instance.
     */
    public static SyncController get(Context context) {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new SyncController(context.getApplicationContext());
        }
        return sInstance;
    }

    /**
     * Open a dialog that gives the user the option to sign in from a list of available accounts.
     *
     * @param fragmentManager the FragmentManager.
     */
    public static void openSigninDialog(FragmentManager fragmentManager) {
        AccountChooserFragment chooserFragment = new AccountChooserFragment();
        chooserFragment.show(fragmentManager, null);
    }

    /**
     * Open a dialog that gives the user the option to sign out.
     *
     * @param fragmentManager the FragmentManager.
     */
    public static void openSignOutDialog(FragmentManager fragmentManager) {
        SignoutFragment signoutFragment = new SignoutFragment();
        signoutFragment.show(fragmentManager, null);
    }

    /**
     * Trigger Chromium sign in of the given account.
     *
     * This also ensure that sync setup is not in progress anymore, so sync will start after
     * sync initialization has happened.
     *
     * @param activity the current activity.
     * @param accountName the full account name.
     */
    public void signIn(Activity activity, String accountName) {
        final Account account = AccountManagerHelper.createAccountFromName(accountName);

        // The SigninManager handles most of the sign-in flow, and doFinishSignIn handles the
        // Chromium testshell specific details.
        SigninManager signinManager = SigninManager.get(mContext);
        final boolean passive = false;
        signinManager.startSignIn(activity, account, passive, new SigninManager.Observer() {
            @Override
            public void onSigninComplete() {
                ProfileSyncService.get(mContext).setSetupInProgress(false);
                // The SigninManager does not control the Android sync state.
                SyncStatusHelper.get(mContext).enableAndroidSync(account);
            }

            @Override
            public void onSigninCancelled() {
            }
        });
    }
}
