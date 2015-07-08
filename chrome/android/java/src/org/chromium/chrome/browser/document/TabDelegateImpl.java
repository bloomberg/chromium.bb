// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;

import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.tab.ChromeTab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.components.service_tab_launcher.ServiceTabLauncher;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;

import java.lang.ref.WeakReference;

import javax.annotation.Nullable;

/**
 * Asynchronously creates Tabs by creating/starting up Activities.
 * TODO(dfalcantara): delete or refactor this after upstreaming.
 */
public class TabDelegateImpl implements TabDelegate {

    private static final String TAG = "cr.document";
    private boolean mIsIncognito;

    /**
     * Creates a TabDelegate.
     * @param incognito Whether or not the TabDelegate handles the creation of incognito tabs.
     */
    public TabDelegateImpl(boolean incognito) {
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
        return ChromeTab.createFrozenTabFromState(id, null, state.isIncognito(), null,
                Tab.INVALID_TAB_ID, state);
    }

    @Override
    public Tab createTabWithWebContents(
            WebContents webContents, int parentId, TabLaunchType type) {
        createTabWithWebContents(webContents, parentId, type, webContents.getUrl());
        return null;
    }

    @Override
    public Tab createTabWithWebContents(
            WebContents webContents, int parentId, TabLaunchType type, String url) {
        createTabWithWebContents(
                webContents, parentId, type, url, DocumentMetricIds.STARTED_BY_WINDOW_OPEN);
        return null;
    }

    /**
     * Creates a Tab to host the given WebContents asynchronously.
     * @param webContents WebContents that has been pre-created.
     * @param parentId ID of the parent Tab.
     * @param type Launch type for the Tab.
     * @param url URL to display in the WebContents.
     * @param startedBy See {@link DocumentMetricIds}.
     */
    public void createTabWithWebContents(
            WebContents webContents, int parentId, TabLaunchType type, String url, int startedBy) {
        // Tabs can't be launched in affiliated mode when a webcontents exists.
        assert type != TabLaunchType.FROM_LONGPRESS_BACKGROUND;

        if (url == null) url = "";

        Context context = ApplicationStatus.getApplicationContext();
        int pageTransition = startedBy == DocumentMetricIds.STARTED_BY_CHROME_HOME_RECENT_TABS
                ? PageTransition.RELOAD : PageTransition.AUTO_TOPLEVEL;
        // TODO(dfalcantara): Pipe information about paused WebContents from native.
        boolean isWebContentsPaused =
                startedBy != DocumentMetricIds.STARTED_BY_CHROME_HOME_RECENT_TABS;

        Activity parentActivity = getActivityForTabId(parentId);

        if (FeatureUtilities.isDocumentMode(context)) {
            PendingDocumentData data = new PendingDocumentData();
            data.webContents = webContents;
            data.webContentsPaused = isWebContentsPaused;

            ChromeLauncherActivity.launchDocumentInstance(parentActivity, mIsIncognito,
                    ChromeLauncherActivity.LAUNCH_MODE_FOREGROUND, url, startedBy, pageTransition,
                    data);
        } else {
            // TODO(dfalcantara): Unify the document-mode path with this path so that both go
            //                    through the ChromeLauncherActivity via an Intent.
            Intent tabbedIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            tabbedIntent.setClass(context, ChromeLauncherActivity.class);
            tabbedIntent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, mIsIncognito);
            tabbedIntent.putExtra(IntentHandler.EXTRA_WEB_CONTENTS, webContents);
            tabbedIntent.putExtra(IntentHandler.EXTRA_WEB_CONTENTS_PAUSED, isWebContentsPaused);
            tabbedIntent.putExtra(IntentHandler.EXTRA_PAGE_TRANSITION_TYPE, pageTransition);
            tabbedIntent.putExtra(IntentHandler.EXTRA_PARENT_TAB_ID, parentId);

            if (parentActivity != null && parentActivity.getIntent() != null) {
                tabbedIntent.putExtra(
                        IntentHandler.EXTRA_PARENT_INTENT, parentActivity.getIntent());
            }

            tabbedIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentHandler.startActivityForTrustedIntent(tabbedIntent, context);
        }
    }

    @Override
    public Tab launchUrl(String url, TabLaunchType type) {
        return createNewTab(new LoadUrlParams(url), type, null);
    }

    @Override
    public Tab launchNTP() {
        return createNewTab(new LoadUrlParams(UrlConstants.NTP_URL, PageTransition.AUTO_TOPLEVEL),
                TabLaunchType.FROM_MENU_OR_OVERVIEW, null);
    }

    @Override
    public Tab createNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent) {
        AsyncTabCreationParams asyncParams = new AsyncTabCreationParams();

        // Figure out how the page will be launched.
        if (TextUtils.equals(UrlConstants.NTP_URL, loadUrlParams.getUrl())) {
            asyncParams.documentLaunchMode = ChromeLauncherActivity.LAUNCH_MODE_RETARGET;
        } else if (type == TabLaunchType.FROM_LONGPRESS_BACKGROUND) {
            if (!parent.isIncognito() && mIsIncognito) {
                // Incognito tabs opened from regular tabs open in the foreground for privacy
                // concerns.
                asyncParams.documentLaunchMode = ChromeLauncherActivity.LAUNCH_MODE_FOREGROUND;
            } else {
                asyncParams.documentLaunchMode = ChromeLauncherActivity.LAUNCH_MODE_AFFILIATED;
            }
        }

        // Classify the startup type.
        if (parent != null && TextUtils.equals(UrlConstants.NTP_URL, parent.getUrl())) {
            asyncParams.documentStartedBy =
                    DocumentMetricIds.STARTED_BY_CHROME_HOME_MOST_VISITED;
        } else if (type == TabLaunchType.FROM_LONGPRESS_BACKGROUND
                || type == TabLaunchType.FROM_LONGPRESS_FOREGROUND) {
            asyncParams.documentStartedBy = DocumentMetricIds.STARTED_BY_CONTEXT_MENU;
        } else if (type == TabLaunchType.FROM_MENU_OR_OVERVIEW) {
            asyncParams.documentStartedBy = DocumentMetricIds.STARTED_BY_OPTIONS_MENU;
        }

        // Tab is created aysnchronously.  Can't return anything, yet.
        createNewTab(loadUrlParams, type, parent, asyncParams);
        return null;
    }

    @Override
    public void createNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, @Nullable Tab parent,
            AsyncTabCreationParams asyncParams) {
        assert asyncParams != null;

        if (FeatureUtilities.isDocumentMode(ApplicationStatus.getApplicationContext())) {
            // Pack up everything.
            PendingDocumentData params = null;
            if (loadUrlParams.getPostData() != null
                    || loadUrlParams.getVerbatimHeaders() != null
                    || loadUrlParams.getReferrer() != null
                    || asyncParams.requestId != null) {
                params = new PendingDocumentData();
                params.postData = loadUrlParams.getPostData();
                params.extraHeaders = loadUrlParams.getVerbatimHeaders();
                params.referrer = loadUrlParams.getReferrer();
                params.requestId = asyncParams.requestId == null
                        ? 0 : asyncParams.requestId.intValue();
            }

            ChromeLauncherActivity.launchDocumentInstance(getActivityFromTab(parent), mIsIncognito,
                    asyncParams.documentLaunchMode, loadUrlParams.getUrl(),
                    asyncParams.documentStartedBy, loadUrlParams.getTransitionType(), params);
        } else {
            // TODO(dfalcantara): Start parceling all of the LoadUrlParams when ChromeTabbedActivity
            //                    can use them.  Use the same non-duplication mechanism as the
            //                    WebContents so that they don't get reused across process restarts.
            Context context = ApplicationStatus.getApplicationContext();

            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(loadUrlParams.getUrl()));
            intent.setClass(context, ChromeLauncherActivity.class);
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, mIsIncognito);
            intent.putExtra(
                    IntentHandler.EXTRA_PAGE_TRANSITION_TYPE, loadUrlParams.getTransitionType());

            if (parent != null) {
                intent.putExtra(IntentHandler.EXTRA_PARENT_TAB_ID, parent.getId());
            }

            if (asyncParams.requestId != null) {
                intent.putExtra(ServiceTabLauncher.LAUNCH_REQUEST_ID_EXTRA,
                        asyncParams.requestId.intValue());
            }

            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentHandler.startActivityForTrustedIntent(intent, context);
        }
    }

    private Activity getActivityFromTab(Tab tab) {
        return tab == null || tab.getWindowAndroid() == null
                ? null : tab.getWindowAndroid().getActivity().get();
    }

    /** @return Running Activity that owns the given Tab, null if the Activity couldn't be found. */
    private Activity getActivityForTabId(int id) {
        if (id == Tab.INVALID_TAB_ID) return null;

        for (WeakReference<Activity> ref : ApplicationStatus.getRunningActivities()) {
            if (!(ref.get() instanceof ChromeActivity)) continue;

            ChromeActivity activity = (ChromeActivity) ref.get();
            if (activity == null) continue;

            if (activity.getTabModelSelector().getModelForTabId(id) != null) return activity;
        }
        return null;
    }
}