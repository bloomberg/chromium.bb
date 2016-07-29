// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.UiConfig;
import org.chromium.chrome.browser.ntp.cards.MarginResizer;
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

    /** Can the header transition. */
    private boolean mCanTransition = false;

    public SnippetHeaderViewHolder(final NewTabPageRecyclerView recyclerView, UiConfig config) {
        super(LayoutInflater.from(recyclerView.getContext())
                        .inflate(R.layout.new_tab_page_snippets_header, recyclerView, false));
        mMaxSnippetHeaderHeight = itemView.getResources().getDimensionPixelSize(
                R.dimen.snippets_article_header_height);

        mMaxPeekPadding = itemView.getResources().getDimensionPixelSize(
                R.dimen.snippets_padding_and_peeking_card_height);

        mRecyclerView = recyclerView;
        MarginResizer.createWithViewAdapter(itemView, config);
    }

    @Override
    public void onBindViewHolder(NewTabPageListItem header) {
        mHeader = (SnippetHeaderListItem) header;
        updateDisplay();
    }

    /**
    * Set whether the header is in a state where the height can vary from show to hidden.
    */
    public void setCanTransition(boolean canTransition) {
        mCanTransition = canTransition;
    }

    /**
     * @return The header height we want to set.
     */
    private int getHeaderHeight() {
        if (!mHeader.isVisible()) return 0;

        // If the header cannot transition but is visible - set the height to the maximum so
        // it always displays
        if (!mCanTransition) return mMaxSnippetHeaderHeight;

        // Check if snippet header top is within range to start showing. Set the header height,
        // this is a percentage of how much is scrolled. The balance of the scroll will be used
        // to display the peeking card.
        int amountScrolled = (mRecyclerView.getHeight() - mMaxPeekPadding) - itemView.getTop();
        return MathUtils.clamp((int) (amountScrolled * SCROLL_HEADER_HEIGHT_PERCENTAGE),
                0, mMaxSnippetHeaderHeight);
    }

    /**
     * Update the view for the fade in/out and heading height.
     */
    public void updateDisplay() {
        RecyclerView.LayoutParams params = (RecyclerView.LayoutParams) itemView.getLayoutParams();
        int headerHeight = getHeaderHeight();

        itemView.setAlpha((float) headerHeight / mMaxSnippetHeaderHeight);
        params.height = headerHeight;
        itemView.requestLayout();
    }
}
