// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.app.Activity;

import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.tab.ChromeTab;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegate;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModel.Entry;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;

/**
 * Provides Tabs to a DocumentTabModel.
 * TODO(dfalcantara): delete or refactor this after upstreaming.
 */
public class TabDelegateImpl implements TabDelegate {
    @Override
    public Tab getActivityTab(
            boolean incognito, ActivityDelegate activityDelegate, Activity activity) {
        if (!(activity instanceof DocumentActivity)
                || !activityDelegate.isValidActivity(incognito, activity.getIntent())) return null;
        return ((DocumentActivity) activity).getActivityTab();
    }

    @Override
    public void createTabInForeground(Activity parentActivity, boolean incognito,
            LoadUrlParams loadUrlParams, PendingDocumentData documentParams) {
        ChromeLauncherActivity.launchDocumentInstance(parentActivity, incognito,
                ChromeLauncherActivity.LAUNCH_MODE_FOREGROUND, loadUrlParams.getUrl(),
                DocumentMetricIds.STARTED_BY_WINDOW_OPEN,
                loadUrlParams.getTransitionType(), false, documentParams);
    }

    @Override
    public Tab createFrozenTab(Entry entry) {
        TabState state = entry.getTabState();
        assert state != null;
        return ChromeTab.createFrozenTabFromState(entry.tabId, null, state.isIncognito(), null,
                Tab.INVALID_TAB_ID, state);
    }

    @Override
    public void createTabWithWebContents(
            boolean isIncognito, WebContents webContents, int parentTabId) {
        PendingDocumentData data = new PendingDocumentData();
        data.webContents = webContents;
        ChromeLauncherActivity.launchDocumentInstance(
                null, isIncognito, ChromeLauncherActivity.LAUNCH_MODE_FOREGROUND, null,
                DocumentMetricIds.STARTED_BY_CHROME_HOME_RECENT_TABS,
                PageTransition.RELOAD, false, data);
    }

    @Override
    public void createTabForDevTools(String url) {
        ChromeLauncherActivity.launchDocumentInstance(
                null, false, ChromeLauncherActivity.LAUNCH_MODE_FOREGROUND, url,
                DocumentMetricIds.STARTED_BY_CHROME_HOME_RECENT_TABS,
                PageTransition.RELOAD, false, null);
    }
}