// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.UiConfig;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.ntp.cards.NewTabPageRecyclerView;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * Provides content to be displayed inside of the Home tab of bottom sheet.
 *
 * TODO(dgn): If the bottom sheet view is not recreated across tab changes, it will have to be
 * notified of it, at least when it is pulled up on the new tab.
 */
public class SuggestionsBottomSheetContent {
    private final NewTabPageRecyclerView mRecyclerView;
    private final ContextMenuManager mContextMenuManager;
    private final SuggestionsUiDelegateImpl mSuggestionsManager;
    private final SnippetsBridge mSnippetsBridge;

    public SuggestionsBottomSheetContent(
            ChromeActivity activity, Tab tab, TabModelSelector tabModelSelector) {
        mRecyclerView = (NewTabPageRecyclerView) LayoutInflater.from(activity).inflate(
                R.layout.new_tab_page_recycler_view, null, false);

        Profile profile = Profile.getLastUsedProfile();
        UiConfig uiConfig = new UiConfig(mRecyclerView);

        mSnippetsBridge = new SnippetsBridge(profile);
        SuggestionsNavigationDelegate navigationDelegate =
                new SuggestionsNavigationDelegateImpl(activity, profile, tab, tabModelSelector);

        mSuggestionsManager = new SuggestionsUiDelegateImpl(
                mSnippetsBridge, mSnippetsBridge, navigationDelegate, profile, tab);
        mContextMenuManager = new ContextMenuManager(activity, navigationDelegate, mRecyclerView);

        NewTabPageAdapter adapter = new NewTabPageAdapter(mSuggestionsManager, null, uiConfig,
                OfflinePageBridge.getForProfile(profile), mContextMenuManager);
        mRecyclerView.setAdapter(adapter);
        mRecyclerView.setUpSwipeToDismiss();
    }

    public RecyclerView getScrollingContentView() {
        return mRecyclerView;
    }

    public ContextMenuManager getContextMenuManager() {
        return mContextMenuManager;
    }

    public void destroy() {
        mSnippetsBridge.destroy();
        mSuggestionsManager.onDestroy();
    }
}
