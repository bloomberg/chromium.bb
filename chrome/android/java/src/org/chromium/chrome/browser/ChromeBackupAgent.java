// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.annotation.TargetApi;
import android.app.backup.BackupAgent;
import android.app.backup.BackupDataInput;
import android.app.backup.BackupDataOutput;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.ParcelFileDescriptor;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferences;
import org.chromium.sync.signin.ChromeSigninController;

import java.io.IOException;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Backup agent for Chrome, filters the restored backup to remove preferences that should not have
 * been restored.
 *
 * Note: Nothing in this class can depend on the ChromeApplication instance having been created.
 * During restore Android creates a special instance of the Chrome application with its own
 * Android defined application class, which is not derived from ChromeApplication.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class ChromeBackupAgent extends BackupAgent {

    private static final String TAG = "ChromeBackupAgent";

    // Lists of preferences that should be restored unchanged.

    // TODO(aberent): At present this only restores the signed in user, and the FRE settings
    // (whether is has been completed, and whether the user disabled crash dump reporting). It
    // should also restore all the user's sync choices. This will involve restoring a number of
    // Chrome preferences (in Chrome's JSON preference file) in addition to the Android preferences
    // currently restored.
    private static final String[] RESTORED_ANDROID_PREFS = {
            PrivacyPreferences.PREF_CRASH_DUMP_UPLOAD,
            FirstRunStatus.FIRST_RUN_FLOW_COMPLETE,
            FirstRunSignInProcessor.FIRST_RUN_FLOW_SIGNIN_SETUP,
    };

    private static boolean sAllowChromeApplication = false;

    @Override
    public void onBackup(ParcelFileDescriptor oldState, BackupDataOutput data,
            ParcelFileDescriptor newState) throws IOException {
        // No implementation needed for Android 6.0 Auto Backup. Used only on older versions of
        // Android Backup
    }

    @Override
    public void onRestore(BackupDataInput data, int appVersionCode, ParcelFileDescriptor newState)
            throws IOException {
        // No implementation needed for Android 6.0 Auto Backup. Used only on older versions of
        // Android Backup
    }

    // May be overriden by downstream products that access account information in a different way.
    protected Account[] getAccounts() {
        Log.d(TAG, "Getting accounts from AccountManager");
        AccountManager manager = (AccountManager) getSystemService(ACCOUNT_SERVICE);
        return manager.getAccounts();
    }

    private boolean accountExistsOnDevice(String userName) {
        // This cannot use AccountManagerHelper, since that depends on ChromeApplication.
        for (Account account: getAccounts()) {
            if (account.name.equals(userName)) return true;
        }
        return false;
    }

    @Override
    public void onRestoreFinished() {
        if (getApplicationContext() instanceof ChromeApplication && !sAllowChromeApplication) {
            // This should never happen in real use, but will happen during testing if Chrome is
            // already running (even in background, started to provide a service, for example).
            Log.w(TAG, "Running with wrong type of Application class");
            return;
        }
        // This is running without a ChromeApplication instance, so this has to be done here.
        ContextUtils.initApplicationContext(getApplicationContext());
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        Set<String> prefNames = sharedPrefs.getAll().keySet();
        // Save the user name for later restoration.
        String userName = sharedPrefs.getString(ChromeSigninController.SIGNED_IN_ACCOUNT_KEY, null);
        Log.d(TAG, "Previous signed in user name = " + userName);
        // If the user hasn't signed in, or can't sign in, then don't restore anything.
        if (userName == null || !accountExistsOnDevice(userName)) {
            boolean commitResult = sharedPrefs.edit().clear().commit();
            Log.d(TAG, "onRestoreFinished complete, nothing restored; commit result = %s",
                    commitResult);
            return;
        }

        SharedPreferences.Editor editor = sharedPrefs.edit();
        // Throw away prefs we don't want to restore.
        Set<String> restoredPrefs = new HashSet<>(Arrays.asList(RESTORED_ANDROID_PREFS));
        for (String pref : prefNames) {
            if (!restoredPrefs.contains(pref)) editor.remove(pref);
        }
        // Because FirstRunSignInProcessor.FIRST_RUN_FLOW_SIGNIN_COMPLETE is not restored Chrome
        // will sign in the user on first run to the account in FIRST_RUN_FLOW_SIGNIN_ACCOUNT_NAME
        // if any. If the rest of FRE has been completed this will happen silently.
        editor.putString(FirstRunSignInProcessor.FIRST_RUN_FLOW_SIGNIN_ACCOUNT_NAME, userName);
        boolean commitResult = editor.commit();

        Log.d(TAG, "onRestoreFinished complete; commit result = %s", commitResult);
    }

    @VisibleForTesting
    static void allowChromeApplicationForTesting() {
        sAllowChromeApplication  = true;
    }
}
