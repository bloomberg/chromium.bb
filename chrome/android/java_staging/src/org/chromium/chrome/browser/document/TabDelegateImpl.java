// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.app.Activity;

import org.chromium.chrome.browser.ChromeMobileApplication;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.tab.ChromeTab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegate;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;

/**
 * Provides Tabs to a DocumentTabModel.
 * TODO(dfalcantara): delete or refactor this after upstreaming.
 */
public class TabDelegateImpl implements TabDelegate {
    private boolean mIsIncognito;

    /**
     * Creates a TabDelegate.
     * @param incognito Whether or not the TabDelegate handles the creation of incognito tabs.
     */
    public TabDelegateImpl(boolean incognito) {
        mIsIncognito = incognito;
    }

    @Override
    public Tab getActivityTab(ActivityDelegate activityDelegate, Activity activity) {
        if (!(activity instanceof DocumentActivity)
                || !activityDelegate.isValidActivity(mIsIncognito, activity.getIntent())) {
            return null;
        }
        return ((DocumentActivity) activity).getActivityTab();
    }

    /**
     * Creates a frozen Tab.  This Tab is not meant to be used or unfrozen -- it is only used as a
     * placeholder until the real Tab can be created.
     * The index is ignored in DocumentMode because Android handles the ordering of Tabs.
     */
    @Override
    public Tab createFrozenTab(TabState state, int id, int index) {
        return ChromeTab.createFrozenTabFromState(id, null, state.isIncognito(), null,
                Tab.INVALID_TAB_ID, state);
    }

    @Override
    public Tab createTabWithWebContents(
            WebContents webContents, int parentId, TabLaunchType type) {
        return createTabWithWebContents(
                webContents, parentId, type, DocumentMetricIds.STARTED_BY_WINDOW_OPEN);
    }

    @Override
    public Tab createTabWithWebContents(
            WebContents webContents, int parentId, TabLaunchType type, int startedBy) {
        // Tabs can't be launched in affiliated mode when a webcontents exists.
        assert type != TabLaunchType.FROM_LONGPRESS_BACKGROUND;

        // TODO(dfalcantara): Pipe information about paused WebContents from native.
        PendingDocumentData data = new PendingDocumentData();
        data.webContents = webContents;
        data.webContentsPaused = startedBy != DocumentMetricIds.STARTED_BY_CHROME_HOME_RECENT_TABS;

        String url = webContents.getUrl();
        if (url == null) url = "";

        // Determine information about the parent Activity.
        Tab parentTab = parentId == Tab.INVALID_TAB_ID
                ? null : ChromeMobileApplication.getDocumentTabModelSelector().getTabById(parentId);
        Activity activity = getActivityFromTab(parentTab);

        int pageTransition = startedBy == DocumentMetricIds.STARTED_BY_CHROME_HOME_RECENT_TABS
                ? PageTransition.RELOAD : PageTransition.AUTO_TOPLEVEL;
        ChromeLauncherActivity.launchDocumentInstance(activity, mIsIncognito,
                ChromeLauncherActivity.LAUNCH_MODE_FOREGROUND, url, startedBy, pageTransition,
                data);

        return null;
    }

    @Override
    public Tab launchUrl(String url, TabLaunchType type) {
        return createNewTab(new LoadUrlParams(url), type, null);
    }

    @Override
    public Tab launchNTP() {
        return launchUrl(UrlConstants.NTP_URL, TabLaunchType.FROM_MENU_OR_OVERVIEW);
    }

    @Override
    public Tab createNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent) {
        PendingDocumentData params = null;
        if (loadUrlParams.getPostData() != null
                || loadUrlParams.getVerbatimHeaders() != null
                || loadUrlParams.getReferrer() != null) {
            params = new PendingDocumentData();
            params.postData = loadUrlParams.getPostData();
            params.extraHeaders = loadUrlParams.getVerbatimHeaders();
            params.referrer = loadUrlParams.getReferrer();
        }

        // TODO(dfalcantara): Is this really only triggered via NTP/Recent Tabs?
        Activity parentActivity = getActivityFromTab(parent);
        int launchMode = type == TabLaunchType.FROM_LONGPRESS_BACKGROUND && !mIsIncognito
                ? ChromeLauncherActivity.LAUNCH_MODE_AFFILIATED
                : ChromeLauncherActivity.LAUNCH_MODE_FOREGROUND;
        ChromeLauncherActivity.launchDocumentInstance(
                parentActivity, mIsIncognito, launchMode, loadUrlParams.getUrl(),
                DocumentMetricIds.STARTED_BY_CHROME_HOME_RECENT_TABS,
                PageTransition.RELOAD, params);
        return null;
    }

    private Activity getActivityFromTab(Tab tab) {
        return tab == null || tab.getWindowAndroid() == null
                ? null : tab.getWindowAndroid().getActivity().get();
    }
}