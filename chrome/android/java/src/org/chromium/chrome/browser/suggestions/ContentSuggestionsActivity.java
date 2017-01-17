// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.Menu;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.SynchronousInitializationActivity;
import org.chromium.chrome.browser.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.favicon.FaviconHelper.IconAvailabilityCallback;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.LogoBridge.LogoObserver;
import org.chromium.chrome.browser.ntp.MostVisitedItem;
import org.chromium.chrome.browser.ntp.NewTabPage.DestructionObserver;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.UiConfig;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.ntp.cards.NewTabPageRecyclerView;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.MostVisitedSites.MostVisitedURLsObserver;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Set;

/**
 * Experimental activity to show content suggestions outside of the New Tab Page.
 */
public class ContentSuggestionsActivity extends SynchronousInitializationActivity {
    private final ObserverList<DestructionObserver> mDestructionObservers = new ObserverList<>();

    private ContextMenuManager mContextMenuManager;
    private SnippetsBridge mSnippetsBridge;
    private NewTabPageRecyclerView mRecyclerView;

    public static void launch(Context context) {
        Intent intent = new Intent();
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setClass(context, ContentSuggestionsActivity.class);
        context.startActivity(intent);
    }

    private class SuggestionsNewTabPageManager implements NewTabPageManager {
        @Override
        public void removeMostVisitedItem(MostVisitedItem item) {}

        @Override
        public void openMostVisitedItem(int windowDisposition, MostVisitedItem item) {}

        @Override
        public boolean isLocationBarShownInNTP() {
            return false;
        }

        @Override
        public boolean isVoiceSearchEnabled() {
            return false;
        }

        @Override
        public boolean isFakeOmniboxTextEnabledTablet() {
            return false;
        }

        @Override
        public boolean isOpenInNewWindowEnabled() {
            return true;
        }

        @Override
        public boolean isOpenInIncognitoEnabled() {
            return true;
        }

        @Override
        public void navigateToBookmarks() {}

        @Override
        public void navigateToRecentTabs() {}

        @Override
        public void navigateToDownloadManager() {}

        @Override
        public void trackSnippetsPageImpression(int[] categories, int[] suggestionsPerCategory) {}

        @Override
        public void trackSnippetImpression(SnippetArticle article) {}

        @Override
        public void trackSnippetMenuOpened(SnippetArticle article) {}

        @Override
        public void trackSnippetCategoryActionImpression(int category, int position) {}

        @Override
        public void trackSnippetCategoryActionClick(int category, int position) {}

        @Override
        public void openSnippet(
                int windowOpenDisposition, SnippetArticle article, int categoryIndex) {}

        @Override
        public void focusSearchBox(boolean beginVoiceSearch, String pastedText) {}

        @Override
        public void setMostVisitedURLsObserver(MostVisitedURLsObserver observer, int numResults) {}

        @Override
        public void getLocalFaviconImageForURL(
                String url, int size, FaviconImageCallback faviconCallback) {}

        @Override
        public void getLargeIconForUrl(String url, int size, LargeIconCallback callback) {}

        @Override
        public void ensureIconIsAvailable(String pageUrl, String iconUrl, boolean isLargeIcon,
                boolean isTemporary, IconAvailabilityCallback callback) {}

        @Override
        public void getUrlsAvailableOffline(Set<String> pageUrls, Callback<Set<String>> callback) {}

        @Override
        public void onLogoClicked(boolean isAnimatedLogoShowing) {}

        @Override
        public void getSearchProviderLogo(LogoObserver logoObserver) {}

        @Override
        public void onLoadingComplete(MostVisitedItem[] mostVisitedItems) {}

        @Override
        public void onLearnMoreClicked() {}

        @Override
        public SuggestionsSource getSuggestionsSource() {
            return mSnippetsBridge;
        }

        @Override
        public void addDestructionObserver(DestructionObserver destructionObserver) {
            mDestructionObservers.addObserver(destructionObserver);
        }

        @Override
        public boolean isCurrentPage() {
            return true;
        }

        @Override
        public ContextMenuManager getContextMenuManager() {
            return mContextMenuManager;
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        assert ChromeFeatureList.isEnabled(ChromeFeatureList.NTP_SUGGESTIONS_STANDALONE_UI);

        mRecyclerView = (NewTabPageRecyclerView) LayoutInflater.from(this).inflate(
                R.layout.new_tab_page_recycler_view, null, false);

        Profile profile = Profile.getLastUsedProfile();
        mSnippetsBridge = new SnippetsBridge(profile);

        NewTabPageManager manager = new SuggestionsNewTabPageManager();
        mContextMenuManager = new ContextMenuManager(this, manager, mRecyclerView);
        UiConfig uiConfig = new UiConfig(mRecyclerView);
        NewTabPageAdapter adapter = new NewTabPageAdapter(
                manager, null, uiConfig, OfflinePageBridge.getForProfile(profile));
        mRecyclerView.setAdapter(adapter);
        mRecyclerView.setUpSwipeToDismiss();

        setContentView(mRecyclerView);
    }

    @Override
    public void onContextMenuClosed(Menu menu) {
        mContextMenuManager.onContextMenuClosed();
    }

    @Override
    protected void onDestroy() {
        for (DestructionObserver observer : mDestructionObservers) {
            observer.onDestroy();
        }
        super.onDestroy();
    }
}
