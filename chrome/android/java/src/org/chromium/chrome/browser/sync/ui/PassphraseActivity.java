// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.accounts.Account;
import android.app.Dialog;
import android.app.DialogFragment;
import android.app.Fragment;
import android.app.FragmentManager;
import android.app.FragmentTransaction;
import android.app.ProgressDialog;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.util.Log;

import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromiumApplication;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.SyncController;
import org.chromium.sync.signin.ChromeSigninController;

/**
 * This activity is used for requesting a sync passphrase or password from the user. Typically,
 * this will be the target of an Android notification.
 */
public class PassphraseActivity extends FragmentActivity implements
        PassphraseDialogFragment.Listener,
        FragmentManager.OnBackStackChangedListener {

    public static final String FRAGMENT_PASSWORD = "password_fragment";
    public static final String FRAGMENT_SPINNER = "spinner_fragment";
    private static final String TAG = "PassphraseActivity";
    private static ProfileSyncService.SyncStateChangedListener sSyncStateChangedListener;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // The Chrome browser process must be started here because this Activity
        // may be started explicitly from Android notifications.
        // During a normal user flow the ChromeTabbedActivity would start the Chrome browser
        // process and this wouldn't be necessary.
        try {
            ((ChromiumApplication) getApplication())
                    .startBrowserProcessesAndLoadLibrariesSync(this, true);
        } catch (ProcessInitException e) {
            Log.e(TAG, "Failed to start browser process.", e);
            ChromiumApplication.reportStartupErrorAndExit(e);
        }
        getFragmentManager().addOnBackStackChangedListener(this);
    }

    @Override
    protected void onResume() {
        super.onResume();
        Account account = ChromeSigninController.get(this).getSignedInUser();
        if (account == null) {
            finish();
            return;
        }

        if (!isShowingDialog(FRAGMENT_PASSWORD)) {
            if (ProfileSyncService.get(this).isSyncInitialized()) {
                displayPasswordDialog();
            } else {
                addSyncStateChangedListener();
                displaySpinnerDialog();
            }
        }
    }

    private void addSyncStateChangedListener() {
        if (sSyncStateChangedListener != null) {
            return;
        }
        sSyncStateChangedListener = new ProfileSyncService.SyncStateChangedListener() {
            @Override
            public void syncStateChanged() {
                if (ProfileSyncService.get(getApplicationContext()).isSyncInitialized()) {
                    removeSyncStateChangedListener();
                    displayPasswordDialog();
                }
            }
        };
        ProfileSyncService.get(this).addSyncStateChangedListener(sSyncStateChangedListener);
    }

    private void removeSyncStateChangedListener() {
        ProfileSyncService.get(this).removeSyncStateChangedListener(sSyncStateChangedListener);
        sSyncStateChangedListener = null;
    }

    private boolean isShowingDialog(String tag) {
        return getFragmentManager().findFragmentByTag(tag) != null;
    }

    private void displayPasswordDialog() {
        assert ProfileSyncService.get(this).isSyncInitialized();
        boolean isGaia = !ProfileSyncService.get(this).isUsingSecondaryPassphrase();
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        ft.addToBackStack(null);
        PassphraseDialogFragment.newInstance(null, isGaia, false).show(ft, FRAGMENT_PASSWORD);
    }

    private void displaySpinnerDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        ft.addToBackStack(null);
        SpinnerDialogFragment dialog = new SpinnerDialogFragment();
        dialog.show(ft, FRAGMENT_SPINNER);
    }

    @Override
    public void onPassphraseEntered(String password, boolean isGaia, boolean isUpdate) {
        if (!password.isEmpty() && ProfileSyncService.get(this).setDecryptionPassphrase(password)) {
            // The passphrase was correct - close this activity.
            finish();
        } else {
            // Invalid passphrase - display an error.
            Fragment fragment = getFragmentManager().findFragmentByTag(FRAGMENT_PASSWORD);
            if (fragment instanceof PassphraseDialogFragment) {
                ((PassphraseDialogFragment) fragment).invalidPassphrase();
            }
        }
    }

    @Override
    public void onPassphraseCanceled(boolean isGaia, boolean isUpdate) {
        // Re add the notification.
        SyncController.get(this).getSyncNotificationController().syncStateChanged();
        finish();
    }

    @Override
    public void onBackStackChanged() {
        if (getFragmentManager().getBackStackEntryCount() == 0) {
            finish();
        }
    }

    /**
     * Dialog shown while sync is loading.
     */
    public static class SpinnerDialogFragment extends DialogFragment {
        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            ProgressDialog dialog = new ProgressDialog(getActivity());
            dialog.setMessage(getResources().getString(R.string.sync_loading));
            return dialog;
        }
    }
}
