// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.ContextUtils;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.prerender.ExternalPrerenderHandler;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.WindowAndroid;

/**
 * Holds a hidden tab which may be used to preload pages before a CustomTabActivity is launched.
 *
 * Lifecycle: 1:1 relationship between this and {@link CustomTabsConnection}.
 * Thread safety: Only access on UI Thread.
 * Native: This class needs native to be loaded (since it creates Tabs).
 */
public class HiddenTabHolder {
    /** Holds the parameters for the current hidden tab speculation. */
    @VisibleForTesting
    static final class SpeculationParams {
        public final CustomTabsSessionToken session;
        public final String url;
        public final Tab tab;
        public final TabObserver observer;
        public final String referrer;

        private SpeculationParams(CustomTabsSessionToken session, String url, Tab tab,
                TabObserver observer, String referrer) {
            this.session = session;
            this.url = url;
            this.tab = tab;
            this.observer = observer;
            this.referrer = referrer;
        }
    }

    private class HiddenTabObserver extends EmptyTabObserver {
        @Override
        public void onCrash(Tab tab) {
            destroyHiddenTab(null);
        }
    }

    @Nullable private SpeculationParams mSpeculation;

    /**
     * Creates a hidden tab and initiates a navigation.
     *
     * @param session The {@link CustomTabsSessionToken} for the Tab to be associated with.
     * @param clientManager The {@link ClientManager} to get referrer information and link
     *                      PostMessage.
     * @param url The URL to load into the Tab.
     * @param extras Extras to be passed that may contain referrer information.
     */
    void launchUrlInHiddenTab(CustomTabsSessionToken session, ClientManager clientManager,
            String url, @Nullable Bundle extras) {
        Intent extrasIntent = new Intent();
        if (extras != null) extrasIntent.putExtras(extras);

        // Ensures no Browser.EXTRA_HEADERS were in the Intent.
        if (IntentHandler.getExtraHeadersFromIntent(extrasIntent) != null) return;

        Tab tab = buildDetachedTab();

        HiddenTabObserver observer = new HiddenTabObserver();
        tab.addObserver(observer);

        // Updating post message as soon as we have a valid WebContents.
        clientManager.resetPostMessageHandlerForSession(session, tab.getWebContents());

        LoadUrlParams loadParams = new LoadUrlParams(url);
        String referrer = IntentHandler.getReferrerUrlIncludingExtraHeaders(extrasIntent);
        if (referrer == null && clientManager.getReferrerForSession(session) != null) {
            referrer = clientManager.getReferrerForSession(session).getUrl();
        }
        if (referrer == null) referrer = "";
        if (!referrer.isEmpty()) {
            loadParams.setReferrer(new Referrer(referrer, ReferrerPolicy.DEFAULT));
        }

        mSpeculation = new SpeculationParams(session, url, tab, observer, referrer);
        mSpeculation.tab.loadUrl(loadParams);
    }

    /**
     * Creates an instance of a {@link Tab} that is fully detached from any activity.
     * Also performs general tab initialization as well as detached specifics.
     *
     * The current application context must allow the creation of a WindowAndroid.
     *
     * @return The newly created and initialized tab.
     */
    private static Tab buildDetachedTab() {
        Context context = ContextUtils.getApplicationContext();
        Tab tab = new TabBuilder()
                          .setWindow(new WindowAndroid(context))
                          .setLaunchType(TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION)
                          .setDelegateFactory(CustomTabDelegateFactory.createDummy())
                          .setInitiallyHidden(true)
                          .build();

        // Resize the webContent to avoid expensive post load resize when attaching the tab.
        Rect bounds = ExternalPrerenderHandler.estimateContentSize(context, false);
        int width = bounds.right - bounds.left;
        int height = bounds.bottom - bounds.top;
        tab.getWebContents().setSize(width, height);
        ReparentingTask.detach(tab);
        return tab;
    }

    /**
     * Returns the preloaded {@link Tab} if it matches the given |url| and |referrer|. Null if no
     * such {@link Tab}. If a {@link Tab} is preloaded but it does not match, it is discarded.
     *
     * @param session The Binder object identifying a session the hidden tab was created for.
     * @param ignoreFragments Whether to ignore fragments while matching the url.
     * @param url The URL the tab is for.
     * @param referrer The referrer to use for |url|.
     * @return The hidden tab, or null.
     */
    @Nullable Tab takeHiddenTab(@Nullable CustomTabsSessionToken session, boolean ignoreFragments,
            String url, @Nullable String referrer) {
        try (TraceEvent e = TraceEvent.scoped("CustomTabsConnection.takeHiddenTab")) {
            if (mSpeculation == null || session == null) return null;
            if (!session.equals(mSpeculation.session)) return null;

            Tab tab = mSpeculation.tab;
            String speculatedUrl = mSpeculation.url;
            String speculationReferrer = mSpeculation.referrer;

            tab.removeObserver(mSpeculation.observer);
            mSpeculation = null;

            boolean urlsMatch = ignoreFragments
                    ? UrlUtilities.urlsMatchIgnoringFragments(speculatedUrl, url)
                    : TextUtils.equals(speculatedUrl, url);

            if (referrer == null) referrer = "";

            if (urlsMatch && TextUtils.equals(speculationReferrer, referrer)) {
                CustomTabsConnection.recordSpeculationStatusSwapTabTaken();
                return tab;
            } else {
                CustomTabsConnection.recordSpeculationStatusSwapTabNotMatched();
                ((TabImpl) tab).destroy();
                return null;
            }
        }
    }

    /** Cancels the speculation for a given session, or any session if null. */
    void destroyHiddenTab(@Nullable CustomTabsSessionToken session) {
        if (mSpeculation == null) return;
        if (session!= null && !session.equals(mSpeculation.session)) return;

        ((TabImpl) (mSpeculation.tab)).destroy();
        mSpeculation = null;
    }

    /** Gets the url of the current hidden tab, if it exists. */
    @Nullable String getSpeculatedUrl(CustomTabsSessionToken session) {
        if (mSpeculation == null || !mSpeculation.session.equals(session)) {
            return null;
        }
        return mSpeculation.url;
    }

    /** Returns whether there currently is a hidden tab. */
    boolean hasHiddenTab() {
        return mSpeculation != null;
    }

    @VisibleForTesting
    @Nullable SpeculationParams getSpeculationParamsForTesting() {
        return mSpeculation;
    }
}
