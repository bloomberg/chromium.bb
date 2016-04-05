// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.Activity;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.text.TextUtils;

import org.chromium.chrome.browser.preferences.SyncedAccountPreference;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInCallback;
import org.chromium.chrome.browser.sync.ui.ConfirmImportSyncDataDialog;
import org.chromium.chrome.browser.sync.ui.ConfirmImportSyncDataDialog.ImportSyncType;

/**
 * A class that encapsulates the control flow of listeners and callbacks when switching sync
 * accounts.
 *
 * Control flows through the method in this order:
 *   {@link OnPreferenceChangeListener#onPreferenceChange}
 *   {@link ConfirmImportSyncDataDialog.Listener#onConfirm}
 *   {@link SignInCallback#onSignInComplete}
 */
public class SyncAccountSwitcher
        implements OnPreferenceChangeListener, ConfirmImportSyncDataDialog.Listener,
                   SignInCallback {
    private static final String TAG = "SyncAccountSwitcher";

    private final SyncedAccountPreference mSyncedAccountPreference;
    private final Activity mActivity;

    private String mNewAccountName;

    /**
     * Sets up a SyncAccountSwitcher to be ready to accept callbacks.
     * @param activity Activity used to get the context for signin and get the fragmentManager
     *                 for the data sync fragment.
     * @param syncedAccountPreference The preference to update once signin has been completed.
     */
    public SyncAccountSwitcher(Activity activity, SyncedAccountPreference syncedAccountPreference) {
        mActivity = activity;
        mSyncedAccountPreference = syncedAccountPreference;
    }

    @Override
    public boolean onPreferenceChange(Preference p, Object newValue) {
        if (newValue == null) return false;

        mNewAccountName = (String) newValue;
        String currentAccount = mSyncedAccountPreference.getValue();

        if (TextUtils.equals(mNewAccountName, currentAccount)) return false;

        ConfirmImportSyncDataDialog.showNewInstance(currentAccount, mNewAccountName,
                ImportSyncType.SWITCHING_SYNC_ACCOUNTS, mActivity.getFragmentManager(), this);

        // Don't update the selected account in the preference. It will be updated by
        // the call to mSyncAccountListPreference.update() if everything succeeds.
        return false;
    }

    @Override
    public void onConfirm(final boolean wipeData) {
        assert mNewAccountName != null;

        final SigninManager.SignInCallback callback = this;

        // Sign out first to get sync working correctly.
        SigninManager.get(mActivity).signOut(new Runnable() {
            @Override
            public void run() {
                SigninManager.get(mActivity).clearLastSignedInUser();

                if (wipeData) {
                    SyncUserDataWiper.wipeSyncUserData(new Runnable() {
                        @Override
                        public void run() {
                            SigninManager.get(mActivity)
                                    .signIn(mNewAccountName, mActivity, callback);
                        }
                    });
                } else {
                    SigninManager.get(mActivity).signIn(mNewAccountName, mActivity, callback);
                }
            }
        });
    }

    @Override
    public void onCancel() {
        // The user aborted the 'merge data' dialog, there is nothing to do.
    }

    @Override
    public void onSignInComplete() {
        // Update the Preference so it displays the correct account name.
        mSyncedAccountPreference.update();
    }

    @Override
    public void onSignInAborted() {
        // If the user aborted signin, there is nothing to do.
    }
}