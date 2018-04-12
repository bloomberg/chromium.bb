// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.contextual_suggestions.ContextualSuggestionsBridge.ContextualSuggestionsResult;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils;
import org.chromium.content_public.browser.WebContents;

/**
 * A fake {@link ContextualSuggestionsSource} for use in testing.
 */
public class FakeContextualSuggestionsSource extends ContextualSuggestionsSource {
    final static String TEST_TOOLBAR_TITLE = "Test toolbar title";
    final static Integer NUMBER_OF_CLUSTERS = 3;
    final static Integer ARTICLES_PER_CLUSTER = 3;
    // There should be 12 items in the cluster list - 3 articles and a title per cluster.
    final static Integer TOTAL_ITEM_COUNT = 12;

    FakeContextualSuggestionsSource() {
        super(null);
    }

    @Override
    protected void init(Profile profile) {
        // Intentionally do nothing.
    }

    @Override
    public void destroy() {
        // Intentionally do nothing.
    }

    @Override
    public void fetchSuggestionImage(SnippetArticle suggestion, Callback<Bitmap> callback) {}

    @Override
    public void fetchContextualSuggestionImage(
            SnippetArticle suggestion, Callback<Bitmap> callback) {}

    @Override
    public void fetchSuggestionFavicon(SnippetArticle suggestion, int minimumSizePx,
            int desiredSizePx, Callback<Bitmap> callback) {}

    @Override
    void fetchSuggestions(String url, Callback<ContextualSuggestionsResult> callback) {
        callback.onResult(createDummyResults());
    }

    @Override
    void reportEvent(WebContents webContents, @ContextualSuggestionsEvent int eventId) {}

    @Override
    void clearState() {}

    private ContextualSuggestionsResult createDummyResults() {
        ContextualSuggestionsResult result = new ContextualSuggestionsResult(TEST_TOOLBAR_TITLE);
        for (int clusterCount = 0; clusterCount < NUMBER_OF_CLUSTERS; clusterCount++) {
            ContextualSuggestionsCluster cluster =
                    new ContextualSuggestionsCluster("Cluster " + clusterCount);

            for (int articleCount = 0; articleCount < ARTICLES_PER_CLUSTER; articleCount++) {
                cluster.getSuggestions().add(ContentSuggestionsTestUtils.createDummySuggestion(
                        KnownCategories.CONTEXTUAL));
            }
            result.getClusters().add(cluster);
        }

        return result;
    }
}
