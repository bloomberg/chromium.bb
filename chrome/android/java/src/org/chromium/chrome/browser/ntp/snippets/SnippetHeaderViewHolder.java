// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.cards.NewTabPageListItem;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;

/**
 * A class that represents the view for a single card snippet.
 */
public class SnippetHeaderViewHolder extends NewTabPageViewHolder {
    private static final int ANIMATION_DURATION_MS = 300;

    private final int mMaxSnippetHeaderHeight;
    private SnippetHeaderListItem mHeader;
    private Animator mAnimator;

    public static View createView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.new_tab_page_snippets_card, parent, false);
    }

    public SnippetHeaderViewHolder(final View cardView) {
        super(cardView);
        mMaxSnippetHeaderHeight = cardView.getResources().getDimensionPixelSize(
                R.dimen.snippets_article_header_height);
    }

    @Override
    public void onBindViewHolder(NewTabPageListItem header) {
        mHeader = (SnippetHeaderListItem) header;
        animateFade();
    }

    /**
     * Update the view for the scroll-triggered fade in/out animation.
     * @param omniBoxHeight The height of the omnibox displayed over the NTP, we use this to offset
     *          the start point of the transition.
     */
    public void onScrolled(int omniBoxHeight) {
        RecyclerView.LayoutParams params = (RecyclerView.LayoutParams) itemView.getLayoutParams();
        int headerHeight = 0;

        // Measurement used to multiply the max snippet height to get a range on when to start
        // modifying the display of article header.
        final int numberHeaderHeight = 2;
        // Used to indicate when to start modifying the snippet header.
        int heightToStartChangingHeader = mMaxSnippetHeaderHeight * numberHeaderHeight;
        int snippetHeaderTop = itemView.getTop();

        // Check if snippet header top is within range to start showing the snippet header.
        if (snippetHeaderTop < omniBoxHeight + heightToStartChangingHeader) {
            // The amount of space the article header has scrolled into the
            // |heightToStartChangingHeader|.
            int amountScrolledIntoHeaderSpace =
                    heightToStartChangingHeader - (snippetHeaderTop - omniBoxHeight);

            // Remove the |numberHeaderHeight| to get the actual header height we want to
            // display. Never let the height be more than the |maxSnippetHeaderHeight|.
            headerHeight = Math.min(
                    amountScrolledIntoHeaderSpace / numberHeaderHeight, mMaxSnippetHeaderHeight);

        }

        if (mHeader.isVisible()) itemView.setAlpha((float) headerHeight / mMaxSnippetHeaderHeight);
        params.height = headerHeight;
        itemView.setLayoutParams(params);
    }

    /**
     * Make the header fade in or out depending on the model's state.
     * @see SnippetHeaderListItem#isVisible()
     */
    private void animateFade() {
        if (mAnimator != null) mAnimator.cancel();

        float currentAlpha = itemView.getAlpha();
        float targetAlpha = mHeader.isVisible() ? 1f : 0f;

        if (currentAlpha == targetAlpha) return;

        mAnimator = ObjectAnimator.ofFloat(itemView, View.ALPHA, currentAlpha, targetAlpha);
        mAnimator.setDuration(ANIMATION_DURATION_MS);
        mAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animator) {
                mAnimator = null;
            }
        });
        mAnimator.start();
    }
}
