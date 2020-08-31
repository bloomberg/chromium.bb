// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.backup.BackupManager;
import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * Class for watching for changes to the Android preferences that are backed up using Android
 * key/value backup.
 */
@JNINamespace("android")
public class ChromeBackupWatcher {
    private BackupManager mBackupManager;

    @VisibleForTesting
    @CalledByNative
    static ChromeBackupWatcher createChromeBackupWatcher() {
        return new ChromeBackupWatcher();
    }

    private ChromeBackupWatcher() {
        Context context = ContextUtils.getApplicationContext();
        if (context == null) return;

        mBackupManager = new BackupManager(context);
        // Watch the Java preferences that are backed up.
        SharedPreferencesManager sharedPrefs = SharedPreferencesManager.getInstance();
        // If we have never done a backup do one immediately.
        if (!sharedPrefs.readBoolean(ChromePreferenceKeys.BACKUP_FIRST_BACKUP_DONE, false)) {
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                mBackupManager.dataChanged();
            }
            sharedPrefs.writeBoolean(ChromePreferenceKeys.BACKUP_FIRST_BACKUP_DONE, true);
        }
        sharedPrefs.addObserver((key) -> {
            // Update the backup if any of the backed up Android preferences change.
            for (String pref : ChromeBackupAgent.BACKUP_ANDROID_BOOL_PREFS) {
                if (key.equals(pref)) {
                    onBackupPrefsChanged();
                    return;
                }
            }
        });
        // Update the backup if the sign-in status changes.
        IdentityServicesProvider.get().getIdentityManager().addObserver(
                new IdentityManager.Observer() {
                    @Override
                    public void onPrimaryAccountSet(CoreAccountInfo account) {
                        onBackupPrefsChanged();
                    }

                    @Override
                    public void onPrimaryAccountCleared(CoreAccountInfo account) {
                        onBackupPrefsChanged();
                    }
                });
    }

    @CalledByNative
    private void onBackupPrefsChanged() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            mBackupManager.dataChanged();
        }
    }
}
