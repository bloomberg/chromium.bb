// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus.CategoryStatusEnum;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories.KnownCategoriesEnum;

import java.util.List;

/**
 * An interface for classes that provide snippets.
 */
public interface SnippetsSource {
    /**
     * An observer for events in the snippets service.
     */
    public interface SnippetsObserver {
        void onSuggestionsReceived(@KnownCategoriesEnum int category,
                List<SnippetArticleListItem> snippets);

        /** Called when a category changed its status. */
        void onCategoryStatusChanged(@KnownCategoriesEnum int category,
                @CategoryStatusEnum int newStatus);
    }

    /**
     * Tells the source to discard the snippet.
     */
    public void discardSnippet(SnippetArticleListItem snippet);

    /**
     * Fetches the thumbnail image for a snippet.
     */
    public void fetchSnippetImage(SnippetArticleListItem snippet, Callback<Bitmap> callback);

    /**
     * Checks whether a snippet has been visited.
     */
    public void getSnippedVisited(SnippetArticleListItem snippet, Callback<Boolean> callback);

    /**
     * Sets the recipient for the fetched snippets.
     */
    public void setObserver(SnippetsObserver observer);

    /**
     * Gives the reason snippets are disabled.
     */
    public int getCategoryStatus(@KnownCategoriesEnum int category);
}