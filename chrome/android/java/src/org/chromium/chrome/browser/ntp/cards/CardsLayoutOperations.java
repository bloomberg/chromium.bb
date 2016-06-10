// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.View;

import org.chromium.chrome.browser.ntp.MostVisitedLayout;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.snippets.SnippetHeaderViewHolder;

/**
 * This is a class to organise the various layout operations of the New Tab Page while using the
 * cards layout. For example, updating the size of the peeking card or controlling the scroll
 * behaviour.
 */
public class CardsLayoutOperations {
    /**
     * Refresh the peeking state of the first card.
     * @param recyclerView The NewTabPageRecyclerView that contains the cards.
     * @param viewportHeight The height of the containing view, to calculate when the transition
     *          should start.
     */
    public static void updatePeekingCard(
            final NewTabPageRecyclerView recyclerView, int viewportHeight) {
        // Get the first snippet that could display to make the peeking card transition.
        int firstCardIndex = 2; // 0 => above-the-fold, 1 => header, 2 => card
        RecyclerView.ViewHolder viewHolder =
                recyclerView.findViewHolderForAdapterPosition(firstCardIndex);

        if (viewHolder == null || !(viewHolder instanceof CardViewHolder)
                || !viewHolder.itemView.isShown()) {
            return;
        }

        ((CardViewHolder) viewHolder).updatePeek(viewportHeight);
    }

    /**
     * Show the snippets header when the user scrolls down and snippet articles starts reaching the
     * top of the screen.
     * @param recyclerView The NewTabPageRecyclerView that contains the header card.
     * @param omniBoxHeight The height of the omnibox displayed over the NTP, we use this to offset
     *          the start point of the transition.
     */
    public static void updateSnippetsHeaderDisplay(NewTabPageRecyclerView recyclerView,
            int omniBoxHeight) {
        // Get the snippet header view. It is always at position 1
        ViewHolder viewHolder = recyclerView.findViewHolderForAdapterPosition(1);

        // Start doing the calculations if the snippet header is currently shown on screen.
        if (viewHolder == null || !(viewHolder instanceof SnippetHeaderViewHolder)) return;

        ((SnippetHeaderViewHolder) viewHolder).onScrolled(omniBoxHeight);

        // Update the space at the bottom, which needs to know about the height of the header.
        recyclerView.refreshBottomSpacing();
    }

    /**
     * Snaps the scroll point to the top of the screen, the top of most visited or to articles
     * depending on the current scroll. This function makes gross assumptions about the layout, so
     * will probably need to be changed if the new tab page changes.
     * @param recyclerView The NewTabPageRecyclerView containing everything.
     * @param newTabPageLayout The above the fold content.
     * @param mostVisitedLayout The view to snap to while snapping to most visited.
     * @param currentScroll The current scroll.
     * @return Whether the snap point was above or below the fold.
     */
    public static NewTabPageUma.SnapState snapScroll(NewTabPageRecyclerView recyclerView,
            NewTabPageLayout newTabPageLayout, MostVisitedLayout mostVisitedLayout,
            int currentScroll) {
        // These calculations only work if the first item is visible (since
        // computeVerticalScrollOffset only takes into account visible items).
        // Luckily, we only need to perform the calculations if the first item is visible.
        if (!recyclerView.isFirstItemVisible()) {
            return NewTabPageUma.SnapState.BELOW_THE_FOLD;
        }

        // If snapping to Most Likely or to Articles, the omnibox will be at the top of the
        // page, so offset the scroll so the scroll targets appear below it.
        final int omniBoxHeight = newTabPageLayout.getPaddingTop();
        final int topOfMostLikelyScroll = mostVisitedLayout.getTop() - omniBoxHeight;
        final int topOfSnippetsScroll = newTabPageLayout.getHeight() - omniBoxHeight;

        assert currentScroll >= 0;
        // Do not do any scrolling if the user is currently viewing articles.
        if (currentScroll >= topOfSnippetsScroll) {
            return NewTabPageUma.SnapState.BELOW_THE_FOLD;
        }

        // If Most Likely is fully visible when we are scrolled to the top, we have two
        // snap points: the Top and Articles.
        // If not, we have three snap points, the Top, Most Likely and Articles.
        boolean snapToMostLikely =
                recyclerView.getHeight() < mostVisitedLayout.getBottom();

        int targetScroll;
        NewTabPageUma.SnapState snapState = NewTabPageUma.SnapState.ABOVE_THE_FOLD;
        if (currentScroll < newTabPageLayout.getHeight() / 3) {
            // In either case, if in the top 1/3 of the original NTP, snap to the top.
            targetScroll = 0;
        } else if (snapToMostLikely
                && currentScroll < newTabPageLayout.getHeight() * 2 / 3) {
            // If in the middle 1/3 and we are snapping to Most Likely, snap to it.
            targetScroll = topOfMostLikelyScroll;
        } else {
            // Otherwise, snap to the Articles.
            targetScroll = topOfSnippetsScroll;
            snapState = NewTabPageUma.SnapState.BELOW_THE_FOLD;
        }
        recyclerView.smoothScrollBy(0, targetScroll - currentScroll);
        return snapState;
    }

    private static View getFirstViewMatchingViewType(RecyclerView recyclerView,
            int newTabPageListItemViewType) {
        for (int i = 0; i < recyclerView.getLayoutManager().getChildCount(); i++) {
            RecyclerView.ViewHolder viewHolder = recyclerView.findViewHolderForLayoutPosition(i);
            if (viewHolder != null && viewHolder.getItemViewType() == newTabPageListItemViewType) {
                return viewHolder.itemView;
            }
        }
        return null;
    }
}
