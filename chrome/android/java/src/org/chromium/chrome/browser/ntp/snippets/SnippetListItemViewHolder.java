// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.chromium.chrome.browser.ntp.snippets.SnippetsManager.SnippetListItem;

/**
 * A class that represents an item we want to display in our list of card snippets.
 */
abstract class SnippetListItemViewHolder extends RecyclerView.ViewHolder {
    private final SnippetsManager mManager;

    /**
     * Constructs a SnippetListItemViewHolder item used to display a component in the list of
     * snippets (e.g., header, article snippet, separator, etc.)
     *
     * @param cardView The View for this item
     * @param manager The SnippetsManager object used to open an article
     */
    public SnippetListItemViewHolder(View itemView, SnippetsManager manager) {
        super(itemView);
        mManager = manager;
    }

    /**
     * Opens the given URL
     *
     * @param url The URL to open.
     */
    protected void loadUrl(String url) {
        mManager.loadUrl(url);
    }

    /**
     * Called when the snippets adapter is requested to update the currently visible ViewHolder with
     * data.
     *
     * @param article The SnippetListItem object that holds the data for this ViewHolder
     */
    public abstract void onBindViewHolder(SnippetListItem snippetItem);
}
