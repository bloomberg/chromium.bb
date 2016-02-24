// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.preferences;

import android.accounts.Account;
import android.content.Context;
import android.content.res.Resources;
import android.preference.Preference;
import android.util.AttributeSet;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.childaccounts.ChildAccountService;
import org.chromium.chrome.browser.sync.GoogleServiceAuthError;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.signin.ChromeSigninController;

/**
 * A preference that displays the current sync account and status (enabled, error, needs passphrase,
 * etc)."
 */
public class SyncPreference extends Preference {
    public SyncPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        updateSyncSummary();
    }

    /**
     * Updates the summary for this preference to reflect the current state of syncing.
     */
    public void updateSyncSummary() {
        setSummary(getSyncStatusSummary(getContext()));
    }

    private static String getSyncStatusSummary(Context context) {
        if (!ChromeSigninController.get(context).isSignedIn()) return "";

        ProfileSyncService profileSyncService = ProfileSyncService.get();
        Resources res = context.getResources();

        if (ChildAccountService.isChildAccount()) {
            return res.getString(R.string.kids_account);
        }

        if (!AndroidSyncSettings.isMasterSyncEnabled(context)) {
            return res.getString(R.string.sync_android_master_sync_disabled);
        }

        if (profileSyncService == null) {
            return res.getString(R.string.sync_is_disabled);
        }

        if (profileSyncService.getAuthError() != GoogleServiceAuthError.State.NONE) {
            return res.getString(profileSyncService.getAuthError().getMessage());
        }

        // TODO(crbug/557784): Surface IDS_SYNC_UPGRADE_CLIENT string when we require the user
        // to upgrade

        boolean syncEnabled = AndroidSyncSettings.isSyncEnabled(context);

        if (syncEnabled) {
            if (!profileSyncService.isBackendInitialized()) {
                return res.getString(R.string.sync_setup_progress);
            }

            if (profileSyncService.isPassphraseRequiredForDecryption()) {
                return res.getString(R.string.sync_need_passphrase);
            }

            Account account = ChromeSigninController.get(context).getSignedInUser();
            return String.format(
                    context.getString(R.string.account_management_sync_summary), account.name);
        }

        return context.getString(R.string.sync_is_disabled);
    }
}
