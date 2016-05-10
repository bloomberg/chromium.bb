// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.document.DocumentMetricIds;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.components.service_tab_launcher.ServiceTabLauncher;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;

/**
 * Asynchronously creates Tabs by creating/starting up Activities.
 */
public class TabDelegate extends TabCreator {
    private final boolean mIsIncognito;

    /**
     * Creates a TabDelegate.
     * @param incognito Whether or not the TabDelegate handles the creation of incognito tabs.
     */
    public TabDelegate(boolean incognito) {
        mIsIncognito = incognito;
    }

    @Override
    public boolean createsTabsAsynchronously() {
        return true;
    }

    /**
     * Creates a frozen Tab.  This Tab is not meant to be used or unfrozen -- it is only used as a
     * placeholder until the real Tab can be created.
     * The index is ignored in DocumentMode because Android handles the ordering of Tabs.
     */
    @Override
    public Tab createFrozenTab(TabState state, int id, int index) {
        return Tab.createFrozenTabFromState(id, null, state.isIncognito(), null,
                Tab.INVALID_TAB_ID, state);
    }

    @Override
    public boolean createTabWithWebContents(Tab parent, WebContents webContents, int parentId,
            TabLaunchType type, String url) {
        return createTabWithWebContents(
                webContents, parentId, type, url, DocumentMetricIds.STARTED_BY_WINDOW_OPEN);
    }

    /**
     * Creates a Tab to host the given WebContents asynchronously.
     * @param webContents   WebContents that has been pre-created.
     * @param parentId      ID of the parent Tab.
     * @param type          Launch type for the Tab.
     * @param url           URL that the WebContents was opened for.
     * @param startedBy     See {@link DocumentMetricIds}.
     */
    public boolean createTabWithWebContents(
            WebContents webContents, int parentId, TabLaunchType type, String url, int startedBy) {
        if (url == null) url = "";

        // TODO(dfalcantara): Does this transition make sense? (crbug.com/509886)
        int pageTransition = startedBy == DocumentMetricIds.STARTED_BY_CHROME_HOME_RECENT_TABS
                ? PageTransition.RELOAD : PageTransition.AUTO_TOPLEVEL;

        AsyncTabCreationParams asyncParams =
                new AsyncTabCreationParams(new LoadUrlParams(url, pageTransition), webContents);
        asyncParams.setDocumentStartedBy(startedBy);
        createNewTab(asyncParams, type, parentId);
        return true;
    }

    /**
     * Creates a tab in the "other" window in multi-window mode. This will only work if
     * {@link MultiWindowUtils#isOpenInOtherWindowSupported} is true for the given activity.
     *
     * @param loadUrlParams Parameters specifying the URL to load and other navigation details.
     * @param activity      The current {@link Activity}
     * @param parentId      The ID of the parent tab, or {@link Tab#INVALID_TAB_ID}.
     */
    public void createTabInOtherWindow(LoadUrlParams loadUrlParams, Activity activity,
            int parentId) {
        Intent intent = createNewTabIntent(new AsyncTabCreationParams(loadUrlParams), parentId);

        Class<?> targetActivity =
                MultiWindowUtils.getInstance().getOpenInOtherWindowActivity(activity);
        if (targetActivity == null) return;
        intent.setClass(activity, targetActivity);

        intent.addFlags(MultiWindowUtils.FLAG_ACTIVITY_LAUNCH_ADJACENT);
        IntentHandler.addTrustedIntentExtras(intent, activity);
        activity.startActivity(intent);
    }

    @Override
    public Tab launchUrl(String url, TabLaunchType type) {
        return createNewTab(new LoadUrlParams(url), type, null);
    }

    @Override
    public Tab createNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent) {
        AsyncTabCreationParams asyncParams = new AsyncTabCreationParams(loadUrlParams);

        // Figure out how the page will be launched.
        if (TextUtils.equals(UrlConstants.NTP_URL, loadUrlParams.getUrl())) {
            asyncParams.setDocumentLaunchMode(ChromeLauncherActivity.LAUNCH_MODE_RETARGET);
        } else if (type == TabLaunchType.FROM_LONGPRESS_BACKGROUND) {
            if (!parent.isIncognito() && mIsIncognito) {
                // Incognito tabs opened from regular tabs open in the foreground for privacy
                // concerns.
                asyncParams.setDocumentLaunchMode(ChromeLauncherActivity.LAUNCH_MODE_FOREGROUND);
            } else {
                asyncParams.setDocumentLaunchMode(ChromeLauncherActivity.LAUNCH_MODE_AFFILIATED);
            }
        }

        // Classify the startup type.
        if (parent != null && TextUtils.equals(UrlConstants.NTP_URL, parent.getUrl())) {
            asyncParams.setDocumentStartedBy(
                    DocumentMetricIds.STARTED_BY_CHROME_HOME_MOST_VISITED);
        } else if (type == TabLaunchType.FROM_LONGPRESS_BACKGROUND
                || type == TabLaunchType.FROM_LONGPRESS_FOREGROUND) {
            asyncParams.setDocumentStartedBy(DocumentMetricIds.STARTED_BY_CONTEXT_MENU);
        } else if (type == TabLaunchType.FROM_CHROME_UI) {
            asyncParams.setDocumentStartedBy(DocumentMetricIds.STARTED_BY_OPTIONS_MENU);
        }

        // Tab is created aysnchronously.  Can't return anything, yet.
        createNewTab(asyncParams, type, parent == null ? Tab.INVALID_TAB_ID : parent.getId());
        return null;
    }

    /**
     * Creates a Tab to host the given WebContents asynchronously.
     * @param asyncParams     Parameters to create the Tab with, including the URL.
     * @param type            Information about how the tab was launched.
     * @param parentId        ID of the parent tab, if it exists.
     */
    public void createNewTab(
            AsyncTabCreationParams asyncParams, TabLaunchType type, int parentId) {
        assert asyncParams != null;

        // Tabs should't be launched in affiliated mode when a webcontents exists.
        assert !(type == TabLaunchType.FROM_LONGPRESS_BACKGROUND
                && asyncParams.getWebContents() != null);

        Intent intent = createNewTabIntent(asyncParams, parentId);
        IntentHandler.startActivityForTrustedIntent(
                intent, ContextUtils.getApplicationContext());
    }

    private Intent createNewTabIntent(AsyncTabCreationParams asyncParams, int parentId) {
        int assignedTabId = TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID);
        AsyncTabParamsManager.add(assignedTabId, asyncParams);

        Intent intent = new Intent(
                Intent.ACTION_VIEW, Uri.parse(asyncParams.getLoadUrlParams().getUrl()));
        intent.setClass(ContextUtils.getApplicationContext(), ChromeLauncherActivity.class);
        intent.putExtra(IntentHandler.EXTRA_TAB_ID, assignedTabId);
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, mIsIncognito);
        intent.putExtra(IntentHandler.EXTRA_PARENT_TAB_ID, parentId);

        Activity parentActivity = ActivityDelegate.getActivityForTabId(parentId);
        if (parentActivity != null && parentActivity.getIntent() != null) {
            intent.putExtra(IntentHandler.EXTRA_PARENT_INTENT, parentActivity.getIntent());
        }

        if (asyncParams.getRequestId() != null) {
            intent.putExtra(ServiceTabLauncher.LAUNCH_REQUEST_ID_EXTRA,
                    asyncParams.getRequestId().intValue());
        }

        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    /**
     * Passes the supplied web app launch intent to the IntentHandler.
     * @param intent Web app launch intent.
     */
    public void createNewStandaloneFrame(Intent intent) {
        assert intent != null;
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                | ApiCompatibilityUtils.getActivityNewDocumentFlag());
        IntentHandler.startActivityForTrustedIntent(intent,
                ContextUtils.getApplicationContext());
    }
}
