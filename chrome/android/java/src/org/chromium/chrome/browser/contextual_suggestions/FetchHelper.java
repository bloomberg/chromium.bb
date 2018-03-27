// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.support.annotation.Nullable;
import android.webkit.URLUtil;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.ui.widget.Toast;

/**
 * A helper class responsible for determining when to trigger requests for suggestions and when to
 * clear state.
 */
class FetchHelper {
    /** A delegate used to fetch suggestions. */
    interface Delegate {
        /**
         * Request a suggestions fetch for the specified {@code url}.
         * @param url The url for which suggestions should be fetched.
         */
        void requestSuggestions(String url);

        /**
         * Called when the state should be reset e.g. when the user navigates away from a webpage.
         */
        void clearState();
    }

    private final Delegate mDelegate;
    private final TabModelSelectorTabModelObserver mTabModelObserver;
    private final TabObserver mTabObserver;

    @Nullable
    private Tab mLastTab;
    @Nullable
    private String mCurrentContextUrl;

    /**
     * Construct a new {@link FetchHelper}.
     * @param delegate The {@link Delegate} used to fetch suggestions.
     * @param tabModelSelector The {@link TabModelSelector} for the containing Activity.
     */
    FetchHelper(Delegate delegate, TabModelSelector tabModelSelector) {
        mDelegate = delegate;

        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onUpdateUrl(Tab tab, String url) {
                refresh(url);
            }
        };

        mTabModelObserver = new TabModelSelectorTabModelObserver(tabModelSelector) {
            @Override
            public void didSelectTab(Tab tab, TabSelectionType type, int lastId) {
                updateCurrentTab(tab);
            }
        };

        updateCurrentTab(tabModelSelector.getCurrentTab());
    }

    void destroy() {
        if (mLastTab != null) {
            mLastTab.removeObserver(mTabObserver);
            mLastTab = null;
        }
        mTabModelObserver.destroy();
    }

    private void refresh(@Nullable final String newUrl) {
        if (!URLUtil.isNetworkUrl(newUrl)) {
            mCurrentContextUrl = "";
            mDelegate.clearState();
            return;
        }

        // Do nothing if the context is the same as the previously requested suggestions.
        if (isContextTheSame(newUrl)) return;

        // Context has changed, so we want to remove any old suggestions.
        mDelegate.clearState();
        mCurrentContextUrl = newUrl;

        Toast.makeText(ContextUtils.getApplicationContext(), "Fetching suggestions...",
                     Toast.LENGTH_SHORT)
                .show();

        mDelegate.requestSuggestions(newUrl);
    }

    private boolean isContextTheSame(String newUrl) {
        return UrlUtilities.urlsMatchIgnoringFragments(newUrl, mCurrentContextUrl);
    }

    /**
     * Update the current tab and refresh suggestions.
     * @param tab The current {@link Tab}.
     */
    private void updateCurrentTab(Tab tab) {
        if (mLastTab != null) mLastTab.removeObserver(mTabObserver);

        mLastTab = tab;
        if (mLastTab == null) return;

        mLastTab.addObserver(mTabObserver);
        refresh(mLastTab.getUrl());
    }
}
