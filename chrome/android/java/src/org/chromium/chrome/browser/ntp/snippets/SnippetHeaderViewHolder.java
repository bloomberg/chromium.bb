// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.cards.NewTabPageListItem;
import org.chromium.chrome.browser.ntp.cards.NewTabPageRecyclerView;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.util.MathUtils;

/**
 * A class that represents the view for a single card snippet.
 */
public class SnippetHeaderViewHolder extends NewTabPageViewHolder {
    private static final double SCROLL_HEADER_HEIGHT_PERCENTAGE = 0.7;

    private final int mMaxSnippetHeaderHeight;
    private final int mMaxPeekPadding;
    private final NewTabPageRecyclerView mRecyclerView;

    private SnippetHeaderListItem mHeader;

    public static View createView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.new_tab_page_snippets_card, parent, false);
    }

    public SnippetHeaderViewHolder(final View cardView, final NewTabPageRecyclerView recyclerView) {
        super(cardView);
        mMaxSnippetHeaderHeight = cardView.getResources().getDimensionPixelSize(
                R.dimen.snippets_article_header_height);

        mMaxPeekPadding = cardView.getResources().getDimensionPixelSize(
                R.dimen.snippets_padding_and_peeking_card_height);

        mRecyclerView = recyclerView;
    }

    @Override
    public void onBindViewHolder(NewTabPageListItem header) {
        mHeader = (SnippetHeaderListItem) header;
        updateDisplay();
    }

    /**
     * Update the view for the fade in/out and heading height.
     */
    public void updateDisplay() {
        RecyclerView.LayoutParams params = (RecyclerView.LayoutParams) itemView.getLayoutParams();
        int headerHeight = 0;
        int snippetHeaderTop = itemView.getTop();
        int recyclerViewHeight = mRecyclerView.getHeight();

        // Check if snippet header top is within range to start showing. Set the header height, this
        // is a percentage of how much is scrolled. The balance of the scroll will
        // be used to display the peeking card.
        int amountScrolled = (recyclerViewHeight - mMaxPeekPadding) - snippetHeaderTop;
        if (mHeader.isVisible()) {
            headerHeight = MathUtils.clamp((int) (amountScrolled * SCROLL_HEADER_HEIGHT_PERCENTAGE),
                    0, mMaxSnippetHeaderHeight);
        }

        itemView.setAlpha((float) headerHeight / mMaxSnippetHeaderHeight);
        params.height = headerHeight;
        itemView.requestLayout();
    }
}
