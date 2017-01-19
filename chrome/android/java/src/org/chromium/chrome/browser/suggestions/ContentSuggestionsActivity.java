// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.Menu;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.SynchronousInitializationActivity;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.UiConfig;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.ntp.cards.NewTabPageRecyclerView;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.lang.ref.WeakReference;

/**
 * Experimental activity to show content suggestions outside of the New Tab Page.
 */
public class ContentSuggestionsActivity extends SynchronousInitializationActivity {
    private static WeakReference<ChromeActivity> sCallerActivity;

    private ContextMenuManager mContextMenuManager;
    private SuggestionsUiDelegateImpl mSuggestionsManager;
    private SnippetsBridge mSnippetsBridge;

    public static void launch(ChromeActivity activity) {
        sCallerActivity = new WeakReference<>(activity);

        Intent intent = new Intent();
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setClass(activity, ContentSuggestionsActivity.class);
        activity.startActivity(intent);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        assert ChromeFeatureList.isEnabled(ChromeFeatureList.NTP_SUGGESTIONS_STANDALONE_UI);

        // TODO(dgn): properly handle retrieving the tab information, or the base activity being
        // destroyed. (https://crbug.com/677672)
        ChromeActivity activity = sCallerActivity.get();
        if (activity == null) throw new IllegalStateException();

        NewTabPageRecyclerView recyclerView =
                (NewTabPageRecyclerView) LayoutInflater.from(this).inflate(
                        R.layout.new_tab_page_recycler_view, null, false);

        Profile profile = Profile.getLastUsedProfile();
        UiConfig uiConfig = new UiConfig(recyclerView);

        Tab currentTab = activity.getActivityTab();
        TabModelSelector tabModelSelector = activity.getTabModelSelector();

        mSnippetsBridge = new SnippetsBridge(profile);
        SuggestionsNavigationDelegate navigationDelegate =
                new SuggestionsNavigationDelegateImpl(this, profile, currentTab, tabModelSelector);

        mSuggestionsManager = new SuggestionsUiDelegateImpl(
                mSnippetsBridge, mSnippetsBridge, navigationDelegate, profile, currentTab);
        mContextMenuManager = new ContextMenuManager(this, navigationDelegate, recyclerView);

        NewTabPageAdapter adapter = new NewTabPageAdapter(mSuggestionsManager, null, uiConfig,
                OfflinePageBridge.getForProfile(profile), mContextMenuManager);
        recyclerView.setAdapter(adapter);
        recyclerView.setUpSwipeToDismiss();

        setContentView(recyclerView);
    }

    @Override
    public void onContextMenuClosed(Menu menu) {
        mContextMenuManager.onContextMenuClosed();
    }

    @Override
    protected void onDestroy() {
        mSnippetsBridge.destroy();
        mSuggestionsManager.onDestroy();
        super.onDestroy();
    }
}
