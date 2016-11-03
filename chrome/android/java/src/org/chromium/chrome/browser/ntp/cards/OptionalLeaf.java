// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;

/**
 * An optional leaf (i.e. single item) in the tree. Depending on its internal state (see
 * {@link #isShown()}), the item will be present or absent from the tree, by manipulating the values
 * returned from {@link ChildNode} methods. This allows the parent node to not have to add or remove
 * the optional leaf from its children manually.
 *
 * For a non optional leaf, see {@link Leaf}. They have similar interfaces.
 */
public abstract class OptionalLeaf extends ChildNode {
    protected OptionalLeaf(NodeParent parent) {
        super(parent);
    }

    @Override
    public int getItemCount() {
        return isShown() ? 1 : 0;
    }

    @Override
    public int getItemViewType(int position) {
        checkIndex(position);
        return getItemViewType();
    }

    @Override
    public void onBindViewHolder(NewTabPageViewHolder holder, int position) {
        checkIndex(position);
        onBindViewHolder(holder);
    }

    @Override
    public SnippetArticle getSuggestionAt(int position) {
        checkIndex(position);
        return null;
    }

    @Override
    public int getDismissSiblingPosDelta(int position) {
        checkIndex(position);
        return 0;
    }

    /**
     * Display the data for this item.
     * @param holder The view holder that should be updated.
     * @see #onBindViewHolder(NewTabPageViewHolder, int)
     * @see android.support.v7.widget.RecyclerView.Adapter#onBindViewHolder
     */
    protected abstract void onBindViewHolder(NewTabPageViewHolder holder);

    /**
     * @return The view type of this item.
     * @see android.support.v7.widget.RecyclerView.Adapter#getItemViewType
     */
    @ItemViewType
    protected abstract int getItemViewType();

    /** @return Whether the optional item is currently visible. */
    public abstract boolean isShown();

    protected void checkIndex(int position) {
        if (position < 0 || position >= getItemCount()) {
            throw new IndexOutOfBoundsException(position + "/" + getItemCount());
        }
    }
}
