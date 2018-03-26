// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.content.Context;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.webkit.URLUtil;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * A mediator for the contextual suggestions UI component responsible for interacting with
 * the contextual suggestions backend, updating the model, and communicating with the
 * component coordinator(s).
 */
class ContextualSuggestionsMediator {
    private final Context mContext;
    private final ContextualSuggestionsCoordinator mCoordinator;
    private final ContextualSuggestionsModel mModel;
    private final SnippetsBridge mBridge;

    private final TabModelSelectorTabModelObserver mTabModelObserver;
    private final TabObserver mTabObserver;
    private Tab mLastTab;

    @Nullable
    private String mCurrentContextUrl;

    /**
     * Construct a new {@link ContextualSuggestionsMediator}.
     * @param context The {@link Context} used to retrieve resources.
     * @param profile The regular {@link Profile}.
     * @param tabModelSelector The {@link TabModelSelector} for the containing activity.
     * @param coordinator The {@link ContextualSuggestionsCoordinator} for the component.
     * @param model The {@link ContextualSuggestionsModel} for the component.
     */
    ContextualSuggestionsMediator(Context context, Profile profile,
            TabModelSelector tabModelSelector, ContextualSuggestionsCoordinator coordinator,
            ContextualSuggestionsModel model) {
        mContext = context;
        mCoordinator = coordinator;
        mModel = model;

        mBridge = new SnippetsBridge(Profile.getLastUsedProfile());

        // TODO(twellington): Remove the following code for tab observing after triggering logic
        // moves to the C++ layer.
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

    /**
     * @return The {@link SuggestionsSource} used to fetch suggestions.
     *
     * TODO(twellington): This method is needed to construct {@link SuggestionsUiDelegateImpl} in
     * the coordinator. Try to remove this dependency.
     */
    SuggestionsSource getSuggestionsSource() {
        return mBridge;
    }

    /** Destroys the mediator. */
    void destroy() {
        if (mLastTab != null) {
            mLastTab.removeObserver(mTabObserver);
            mLastTab = null;
        }
        mTabModelObserver.destroy();
    }

    // TODO(twellington): Remove this method after triggering logic moves to the C++ layer.
    private void refresh(@Nullable final String newUrl) {
        if (!URLUtil.isNetworkUrl(newUrl)) {
            clearSuggestions();
            return;
        }

        // Do nothing if there are already suggestions in the suggestions list for the new context.
        if (isContextTheSame(newUrl)) return;

        // Context has changed, so we want to remove any old suggestions from the section.
        clearSuggestions();
        mCurrentContextUrl = newUrl;

        Toast.makeText(ContextUtils.getApplicationContext(), "Fetching suggestions...",
                     Toast.LENGTH_SHORT)
                .show();

        mBridge.fetchContextualSuggestions(newUrl, (suggestions) -> {
            // Avoiding double fetches causing suggestions for incorrect context.
            if (!TextUtils.equals(newUrl, mCurrentContextUrl)) return;

            Toast.makeText(ContextUtils.getApplicationContext(),
                         suggestions.size() + " suggestions fetched", Toast.LENGTH_SHORT)
                    .show();

            if (suggestions.size() > 0) {
                displaySuggestions(generateClusterList(suggestions), suggestions.get(0).mTitle);
            }
        });
    }

    private void clearSuggestions() {
        mModel.setClusterList(new ClusterList(Collections.emptyList()));
        mModel.setCloseButtonOnClickListener(null);
        mModel.setTitle(null);
        mCoordinator.removeSuggestions();
    }

    private void displaySuggestions(ClusterList clusters, String title) {
        mModel.setClusterList(clusters);
        mModel.setCloseButtonOnClickListener(view -> { clearSuggestions(); });

        mModel.setTitle(mContext.getString(R.string.contextual_suggestions_toolbar_title, title));

        mCoordinator.displaySuggestions();
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

    // TODO(twellington): Remove after clusters are returned from the backend.
    private ClusterList generateClusterList(List<SnippetArticle> suggestions) {
        List<ContextualSuggestionsCluster> clusters = new ArrayList<>();
        int clusterSize = suggestions.size() >= 6 ? 3 : 2;
        int numClusters = suggestions.size() < 4 ? 1 : suggestions.size() / clusterSize;
        int currentSuggestion = 0;

        // Construct a list of clusters.
        for (int i = 0; i < numClusters; i++) {
            ContextualSuggestionsCluster cluster =
                    new ContextualSuggestionsCluster(suggestions.get(currentSuggestion).mTitle);
            if (i == 0) cluster.setShouldShowTitle(false);

            for (int j = 0; j < clusterSize; j++) {
                cluster.getSuggestions().add(suggestions.get(currentSuggestion));
                currentSuggestion++;
            }

            clusters.add(cluster);
        }

        // Add the remaining suggestions to the last cluster.
        while (currentSuggestion < suggestions.size()) {
            clusters.get(clusters.size() - 1)
                    .getSuggestions()
                    .add(suggestions.get(currentSuggestion));
            currentSuggestion++;
        }

        for (ContextualSuggestionsCluster cluster : clusters) {
            cluster.buildChildren();
        }

        return new ClusterList(clusters);
    }
}
