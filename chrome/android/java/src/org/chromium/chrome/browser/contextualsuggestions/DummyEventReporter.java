// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsuggestions;

import org.chromium.chrome.browser.ntp.cards.ActionItem;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.suggestions.SuggestionsEventReporter;
import org.chromium.chrome.browser.suggestions.SuggestionsRanker;

/** A dummy {@link SuggestionsEventReporter} to use until a real one replaces it. */
class DummyEventReporter implements SuggestionsEventReporter {
    @Override
    public void onSurfaceOpened() {}

    @Override
    public void onPageShown(
            int[] categories, int[] suggestionsPerCategory, boolean[] isCategoryVisible) {}

    @Override
    public void onSuggestionShown(SnippetArticle suggestion) {}

    @Override
    public void onSuggestionOpened(SnippetArticle suggestion, int windowOpenDisposition,
            SuggestionsRanker suggestionsRanker) {}

    @Override
    public void onSuggestionMenuOpened(SnippetArticle suggestion) {}

    @Override
    public void onMoreButtonShown(ActionItem category) {}

    @Override
    public void onMoreButtonClicked(ActionItem category) {}
}