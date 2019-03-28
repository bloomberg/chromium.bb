// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;

import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabNavigationEventObserver;
import org.chromium.chrome.browser.customtabs.CustomTabObserver;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Loads the url received in the Intent as soon as the initial tab is created.
 *
 * Lifecycle: should be created once on Activity creation. When finishes its work, detaches
 * observers, and should get gc-ed. Consider adding @ActivityScope if this policy changes.
 */
public class CustomTabActivityInitialPageLoader {
    private final CustomTabActivityTabProvider mTabProvider;
    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final Lazy<CustomTabObserver> mCustomTabObserver;
    private final CustomTabNavigationEventObserver mNavigationEventObserver;
    private final CustomTabActivityNavigationController mNavigationController;

    @Nullable
    private final String mSpeculatedUrl;

    @Inject
    public CustomTabActivityInitialPageLoader(CustomTabActivityTabProvider tabProvider,
            CustomTabIntentDataProvider intentDataProvider, CustomTabsConnection connection,
            Lazy<CustomTabObserver> customTabObserver,
            CustomTabNavigationEventObserver navigationEventObserver,
            CustomTabActivityNavigationController navigationController) {
        mTabProvider = tabProvider;
        mIntentDataProvider = intentDataProvider;
        mCustomTabObserver = customTabObserver;
        mNavigationEventObserver = navigationEventObserver;
        mNavigationController = navigationController;
        mSpeculatedUrl = connection.getSpeculatedUrl(mIntentDataProvider.getSession());

        mTabProvider.addObserver(new CustomTabActivityTabProvider.Observer() {
            @Override
            public void onInitialTabCreated(@NonNull Tab tab, @TabCreationMode int mode) {
                loadInitialUrlIfNecessary(tab, mode);
                mTabProvider.removeObserver(this);
            }
        });
    }

    private void loadInitialUrlIfNecessary(@NonNull Tab tab, @TabCreationMode int mode) {
        if (mode == TabCreationMode.RESTORED) return;

        if (mode == TabCreationMode.HIDDEN) {
            handleInitialLoadForHiddedTab(tab);
            return;
        }

        LoadUrlParams params = new LoadUrlParams(mIntentDataProvider.getUrlToLoad());

        navigate(params);
    }

    // The hidden tab case needs a bit of special treatment.
    private void handleInitialLoadForHiddedTab(Tab tab) {
        String url = mIntentDataProvider.getUrlToLoad();

        // Manually generating metrics in case the hidden tab has completely finished loading.
        if (!tab.isLoading() && !tab.isShowingErrorPage()) {
            mCustomTabObserver.get().onPageLoadStarted(tab, url);
            mCustomTabObserver.get().onPageLoadFinished(tab, url);
            mNavigationEventObserver.onPageLoadStarted(tab, url);
            mNavigationEventObserver.onPageLoadFinished(tab, url);
        }

        // No actual load to do if the hidden tab already has the exact correct url.
        if (TextUtils.equals(mSpeculatedUrl, url)) {
            return;
        }

        LoadUrlParams params = new LoadUrlParams(url);

        // The following block is a hack that deals with urls preloaded with
        // the wrong fragment. Does an extra pageload and replaces history.
        if (mSpeculatedUrl != null
                && UrlUtilities.urlsFragmentsDiffer(mSpeculatedUrl, url)) {
            params.setShouldReplaceCurrentEntry(true);
        }

        navigate(params);
    }

    private void navigate(LoadUrlParams params) {
        mNavigationController.navigate(params,
                IntentHandler.getTimestampFromIntent(mIntentDataProvider.getIntent()));
    }
}
