// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Intent;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.LayoutInflater;

import org.chromium.base.Log;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.ProfileDataCache;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.ui.SyncCustomizationFragment;

/**
 * An Activity displayed from the MainPreferences to allow the user to pick an account to
 * sign in to. The AccountSigninView.Delegate interface is fulfilled by the AppCompatActivity.
 */
public class AccountSigninActivity extends AppCompatActivity
        implements AccountSigninView.Listener, AccountSigninView.Delegate,
                   SigninManager.SignInCallback{
    private static final String TAG = "SigninActivity";

    private static final String CONFIRM_IMPORT_SYNC_DATA_DIALOG_TAG =
            "signin_import_data_tag";

    private AccountSigninView mView;
    private String mAccountName;
    private boolean mShowSyncSettings = false;

    @Override
    @SuppressFBWarnings("DM_EXIT")
    protected void onCreate(Bundle savedInstanceState) {
        // The browser process must be started here because this activity may be started from the
        // recent apps list and it relies on other activities and the native library to be loaded.
        try {
            ChromeBrowserInitializer.getInstance(this).handleSynchronousStartup();
        } catch (ProcessInitException e) {
            Log.e(TAG, "Failed to start browser process.", e);
            // Since the library failed to initialize nothing in the application
            // can work, so kill the whole application not just the activity
            System.exit(-1);
        }

        // We don't trust android to restore the saved state correctly, so pass null.
        super.onCreate(null);

        mView = (AccountSigninView) LayoutInflater.from(this).inflate(
                R.layout.account_signin_view, null);
        mView.init(new ProfileDataCache(this, Profile.getLastUsedProfile()));
        mView.configureForSettingsPage();
        mView.setListener(this);
        mView.setDelegate(this);

        setContentView(mView);
    }

    @Override
    public void onAccountSelectionCanceled() {
        finish();
    }

    @Override
    public void onNewAccount() {
        AccountAdder.getInstance().addAccount(this, AccountAdder.ADD_ACCOUNT_RESULT);
    }

    @Override
    public void onAccountSelected(String accountName, boolean settingsClicked) {
        mShowSyncSettings = settingsClicked;
        mAccountName = accountName;
        RecordUserAction.record("Signin_Signin_FromSettings");
        SigninManager.get(this).signIn(accountName, this, this);
    }

    @Override
    public void onFailedToSetForcedAccount(String forcedAccountName) {
        assert false : "No forced accounts in account switching preferences.";

    }

    @Override
    public void onSignInComplete() {
        if (mShowSyncSettings) {
            Intent intent = PreferencesLauncher.createIntentForSettingsPage(this,
                    SyncCustomizationFragment.class.getName());
            Bundle args = new Bundle();
            args.putString(SyncCustomizationFragment.ARGUMENT_ACCOUNT, mAccountName);
            intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, args);
            startActivity(intent);
        }

        finish();
    }

    @Override
    public void onSignInAborted() {
        assert false : "Signin cannot be aborted when forced.";
    }
}
