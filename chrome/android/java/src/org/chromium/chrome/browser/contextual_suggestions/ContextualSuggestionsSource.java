// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.List;

/**
 * Provides content for contextual suggestions.
 */
class ContextualSuggestionsSource implements SuggestionsSource {
    private final ContextualSuggestionsBridge mBridge;

    /**
     * Creates a ContextualSuggestionsSource for getting contextual suggestions for the current
     * user.
     *
     * @param profile Profile of the user.
     */
    ContextualSuggestionsSource(Profile profile) {
        mBridge = new ContextualSuggestionsBridge(profile);
    }

    @Override
    public void destroy() {
        mBridge.destroy();
    }

    @Override
    public void fetchSuggestionImage(SnippetArticle suggestion, Callback<Bitmap> callback) {
        mBridge.fetchSuggestionImage(suggestion, callback);
    }

    @Override
    public void fetchContextualSuggestionImage(
            SnippetArticle suggestion, Callback<Bitmap> callback) {
        mBridge.fetchSuggestionImage(suggestion, callback);
    }

    @Override
    public void fetchSuggestionFavicon(SnippetArticle suggestion, int minimumSizePx,
            int desiredSizePx, Callback<Bitmap> callback) {
        mBridge.fetchSuggestionFavicon(suggestion, callback);
    }

    /**
     * @return The {@link ContextualSuggestionsBridge} used communicate with the contextual
     *         suggestions C++ component.
     */
    ContextualSuggestionsBridge getBridge() {
        return mBridge;
    }

    // The following methods are not applicable to contextual suggestions.
    // TODO(twellington): The NTP classes used to display suggestion cards rely
    // on the SuggestionsSource implementation. Refactor to limit reliance to the
    // subset of methods actually used to render cards.

    @Override
    public void fetchRemoteSuggestions() {}

    @Override
    public boolean areRemoteSuggestionsEnabled() {
        return false;
    }

    @Override
    public int[] getCategories() {
        return null;
    }

    @Override
    public int getCategoryStatus(int category) {
        return 0;
    }

    @Override
    public SuggestionsCategoryInfo getCategoryInfo(int category) {
        return null;
    }

    @Override
    public List<SnippetArticle> getSuggestionsForCategory(int category) {
        return null;
    }

    @Override
    public void fetchSuggestions(int category, String[] displayedSuggestionIds,
            Callback<List<SnippetArticle>> successCallback, Runnable failureRunnable) {}

    @Override
    public void fetchContextualSuggestions(String url, Callback<List<SnippetArticle>> callback) {}

    @Override
    public void dismissSuggestion(SnippetArticle suggestion) {}

    @Override
    public void dismissCategory(int category) {}

    @Override
    public void restoreDismissedCategories() {}

    @Override
    public void addObserver(Observer observer) {}

    @Override
    public void removeObserver(Observer observer) {}
}
