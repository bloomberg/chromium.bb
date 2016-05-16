// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.TargetApi;
import android.app.backup.BackupAgent;
import android.app.backup.BackupDataInput;
import android.app.backup.BackupDataOutput;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.preference.PreferenceManager;

import org.chromium.base.Log;
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
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class ChromeBackupAgent extends BackupAgent {

    private static final String TAG = "ChromeBackupAgent";

    // Lists of preferences that should be restored unchanged.

    // TODO(aberent): At present this only restores the signed in user, and the FRE settings
    // (whether is has been completed, and whether the user disabled crash dump reporting). It
    // should restore all non-device specific aspects of the user's state. This will involve both
    // restoring many more Android preferences and many Chrome preferences (in Chrome's JSON
    // preference file).
    private static final String[] RESTORED_ANDROID_PREFS = {
            PrivacyPreferences.PREF_CRASH_DUMP_UPLOAD,
            FirstRunStatus.FIRST_RUN_FLOW_COMPLETE,
            FirstRunSignInProcessor.FIRST_RUN_FLOW_SIGNIN_SETUP,
    };

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

    @Override
    public void onRestoreFinished() {
        SharedPreferences sharedPrefs =
                PreferenceManager.getDefaultSharedPreferences(ChromeBackupAgent.this);
        Set<String> prefNames = sharedPrefs.getAll().keySet();
        // Save the user name for later restoration.
        String userName = sharedPrefs.getString(ChromeSigninController.SIGNED_IN_ACCOUNT_KEY, null);
        SharedPreferences.Editor editor = sharedPrefs.edit();
        // Throw away prefs we don't want to restore.
        Set<String> restoredPrefs = new HashSet<>(Arrays.asList(RESTORED_ANDROID_PREFS));
        for (String pref : prefNames) {
            Log.d(TAG, "Checking pref " + pref);
            if (!restoredPrefs.contains(pref)) editor.remove(pref);
        }
        // Because FirstRunSignInProcessor.FIRST_RUN_FLOW_SIGNIN_COMPLETE is not restored Chrome
        // will sign in the user on first run to the account in FIRST_RUN_FLOW_SIGNIN_ACCOUNT_NAME
        // if any. If the rest of FRE has been completed this will happen silently.
        if (userName != null) {
            editor.putString(FirstRunSignInProcessor.FIRST_RUN_FLOW_SIGNIN_ACCOUNT_NAME, userName);
        }
        boolean commitResult = editor.commit();

        Log.d(TAG, "onRestoreFinished complete; commit result = " + commitResult);
    }
}
