// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.share.ShareActivity;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.components.sync.ModelType;
import org.chromium.ui.widget.Toast;

/**
 * A simple activity that allows Chrome to expose send tab to self as an option in the share menu.
 */
public class SendTabToSelfShareActivity extends ShareActivity {
    @Override
    protected void handleShareAction(ChromeActivity triggeringActivity) {
        Tab tab = triggeringActivity.getActivityTabProvider().getActivityTab();
        SendTabToSelfAndroidBridge.addEntry(tab.getProfile(), tab.getUrl(), tab.getTitle());
        Toast.makeText(triggeringActivity, R.string.send_tab_to_self_toast, Toast.LENGTH_SHORT)
                .show();
    }

    public static boolean featureIsAvailable(Tab currentTab) {
        // Check that sync requirements are met:
        //   User is syncing on at least 2 devices (including this one)
        //   SendTabToSelf sync datatype is enabled
        ProfileSyncService syncService = ProfileSyncService.get();
        boolean userEnabledSyncType =
                syncService.getPreferredDataTypes().contains(ModelType.SEND_TAB_TO_SELF);
        boolean syncRequirementsMet =
                userEnabledSyncType && syncService.getNumberOfSyncedDevices() > 1;

        // Check that the tab and web content requirements are met:
        //   The active tab is not in inCognito mode or on a native page
        //   User is viewing an HTTP or HTTPS page
        boolean isHttpOrHttps = UrlUtilities.isHttpOrHttps(currentTab.getUrl());
        boolean contentRequirementsMet =
                isHttpOrHttps && !currentTab.isNativePage() && !currentTab.isIncognito();

        // Return whether the feature is enabled and the criteria is met as defined above.
        boolean featureEnabled = ChromeFeatureList.isEnabled(ChromeFeatureList.SEND_TAB_TO_SELF);
        return featureEnabled && contentRequirementsMet && syncRequirementsMet;
    }
}
