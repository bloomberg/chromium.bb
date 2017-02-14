// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.suggestions;

import org.chromium.chrome.browser.ntp.cards.ActionItem;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.suggestions.SuggestionsMetricsReporter;
import org.chromium.chrome.browser.suggestions.SuggestionsRanker;

/**
 * Dummy implementation of {@link SuggestionsMetricsReporter} that doesn't do anything.
 */
public class DummySuggestionsMetricsReporter implements SuggestionsMetricsReporter {
    @Override
    public void onPageShown(int[] categories, int[] suggestionsPerCategory) {}

    @Override
    public void onSuggestionShown(SnippetArticle suggestion) {}

    @Override
    public void onSuggestionOpened(SnippetArticle suggestion, int windowOpenDisposition) {}

    @Override
    public void onSuggestionMenuOpened(SnippetArticle suggestion) {}

    @Override
    public void onMoreButtonShown(@CategoryInt ActionItem category) {}

    @Override
    public void onMoreButtonClicked(@CategoryInt ActionItem category) {}

    @Override
    public void setRanker(SuggestionsRanker suggestionsRanker) {}
}
