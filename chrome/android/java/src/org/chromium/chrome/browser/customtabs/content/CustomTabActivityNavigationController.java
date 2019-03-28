// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import android.os.SystemClock;

import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabObserver;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Responsible for navigating to new pages and going back to previous pages.
 * TODO(pshmakov): move back/close navigation from CustomTabActivity into this class.
 */
@ActivityScope
public class CustomTabActivityNavigationController {

    private final CustomTabActivityTabProvider mTabProvider;
    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final CustomTabsConnection mConnection;
    private final Lazy<CustomTabObserver> mCustomTabObserver;

    @Inject
    public CustomTabActivityNavigationController(CustomTabActivityTabProvider tabProvider,
            CustomTabIntentDataProvider intentDataProvider, CustomTabsConnection connection,
            Lazy<CustomTabObserver> customTabObserver) {
        mTabProvider = tabProvider;
        mIntentDataProvider = intentDataProvider;
        mConnection = connection;
        mCustomTabObserver = customTabObserver;
    }

    /**
     * Navigates to given url.
     */
    public void navigate(String url) {
        navigate(new LoadUrlParams(url), SystemClock.elapsedRealtime());
    }

    /**
     * Performs navigation using given {@link LoadUrlParams}.
     * Uses provided timestamp as the initial time for tracking page loading times
     * (see {@link CustomTabObserver}).
     */
    public void navigate(final LoadUrlParams params, long timeStamp) {
        Tab tab = mTabProvider.getTab();
        if (tab == null) {
            assert false;
            return;
        }

        mCustomTabObserver.get().trackNextPageLoadFromTimestamp(tab, timeStamp);

        IntentHandler.addReferrerAndHeaders(params, mIntentDataProvider.getIntent());
        if (params.getReferrer() == null) {
            params.setReferrer(mConnection.getReferrerForSession(mIntentDataProvider.getSession()));
        }

        // See ChromeTabCreator#getTransitionType(). If the sender of the intent was a WebAPK, mark
        // the intent as a standard link navigation. Pass the user gesture along since one must have
        // been active to open a new tab and reach here. Otherwise, mark the navigation chain as
        // starting from an external intent. See crbug.com/792990.
        int defaultTransition = PageTransition.LINK | PageTransition.FROM_API;
        if (mIntentDataProvider.isOpenedByWebApk()) {
            params.setHasUserGesture(true);
            defaultTransition = PageTransition.LINK;
        }
        params.setTransitionType(IntentHandler.getTransitionTypeFromIntent(
                mIntentDataProvider.getIntent(), defaultTransition));
        tab.loadUrl(params);
    }
}
