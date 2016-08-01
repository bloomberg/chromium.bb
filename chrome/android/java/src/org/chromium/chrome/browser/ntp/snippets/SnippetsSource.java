// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.Bitmap;

import org.chromium.base.Callback;

import java.util.List;


/**
 * An interface for classes that provide Snippets.
 */
public interface SnippetsSource {
    /**
     * An observer for events in the snippets service.
     */
    public interface SnippetsObserver {
        void onSnippetsReceived(List<SnippetArticleListItem> snippets);

        /** Called when the ARTICLES category changed its status. */
        void onCategoryStatusChanged(int newStatus);
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
    public int getCategoryStatus();
}