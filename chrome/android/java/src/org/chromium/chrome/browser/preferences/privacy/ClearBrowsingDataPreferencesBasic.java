// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.os.Bundle;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTab;
import org.chromium.chrome.browser.preferences.ClearBrowsingDataCheckBoxPreference;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.ModelType;

import java.util.EnumSet;

/**
 * A simpler version of {@link ClearBrowsingDataPreferences} with fewer dialog options and more
 * explanatory text.
 */
public class ClearBrowsingDataPreferencesBasic extends ClearBrowsingDataPreferences {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        ClearBrowsingDataCheckBoxPreference historyCheckbox =
                (ClearBrowsingDataCheckBoxPreference) findPreference(PREF_HISTORY);
        ClearBrowsingDataCheckBoxPreference cookiesCheckbox =
                (ClearBrowsingDataCheckBoxPreference) findPreference(PREF_COOKIES);

        historyCheckbox.setLinkClickDelegate(() -> {
            new TabDelegate(false /* incognito */)
                    .launchUrl(UrlConstants.MY_ACTIVITY_URL_IN_CBD,
                            TabModel.TabLaunchType.FROM_CHROME_UI);
        });

        if (ChromeSigninController.get().isSignedIn()) {
            if (isHistorySyncEnabled()) {
                historyCheckbox.setSummary(R.string.clear_browsing_history_summary_synced);
            } else {
                historyCheckbox.setSummary(R.string.clear_browsing_history_summary_signed_in);
            }
        }

        // On the basic tab the COOKIES checkbox includes Media Licenses,
        // so update the title to reflect that.
        cookiesCheckbox.setTitle(R.string.clear_cookies_media_licenses_and_site_data_title);
    }

    private boolean isHistorySyncEnabled() {
        boolean syncEnabled = AndroidSyncSettings.isSyncEnabled();
        ProfileSyncService syncService = ProfileSyncService.get();
        return syncEnabled && syncService != null
                && syncService.getActiveDataTypes().contains(ModelType.HISTORY_DELETE_DIRECTIVES);
    }

    @Override
    protected int getPreferenceType() {
        return ClearBrowsingDataTab.BASIC;
    }

    @Override
    protected DialogOption[] getDialogOptions() {
        return new DialogOption[] {DialogOption.CLEAR_HISTORY,
                DialogOption.CLEAR_COOKIES_AND_SITE_DATA, DialogOption.CLEAR_CACHE};
    }

    @Override
    protected int[] getDataTypesFromOptions(EnumSet<DialogOption> options) {
        int[] dataTypes;
        int i = 0;
        if (options.contains(DialogOption.CLEAR_COOKIES_AND_SITE_DATA)) {
            // COOKIES checkbox includes MEDIA_LICENSES, which need to be
            // specified separately. This is only done for the basic tab.
            // On the advanced tab Media Licenses has its own checkbox.
            dataTypes = new int[options.size() + 1];
            dataTypes[i++] = BrowsingDataType.MEDIA_LICENSES;
        } else {
            dataTypes = new int[options.size()];
        }
        for (DialogOption option : options) {
            dataTypes[i++] = option.getDataType();
        }
        return dataTypes;
    }

    @Override
    protected void onClearBrowsingData() {
        super.onClearBrowsingData();
        RecordHistogram.recordEnumeratedHistogram("History.ClearBrowsingData.UserDeletedFromTab",
                ClearBrowsingDataTab.BASIC, ClearBrowsingDataTab.NUM_TYPES);
        RecordUserAction.record("ClearBrowsingData_BasicTab");
    }
}
