// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.MostVisitedLayout;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticleViewHolder;
import org.chromium.chrome.browser.util.MathUtils;

/**
 * This is a class to organise the various layout operations of the New Tab Page while using the
 * cards layout. For example, updating the size of the peeking card or controlling the scroll
 * behaviour.
 */
public class CardsLayoutOperations {

    /**
     * Change the width, padding and child opacity of the first card (the peeking card) to give a
     * smooth transition as the user scrolls. The peeking card will scroll to the Articles on touch.
     * @param recyclerView The NewTabPageRecyclerView that contains the peeking card.
     * @param ntpLayout The NewTabPageLayout to scroll beyond when the peeking card is tapped.
     * @param viewportHeight The height of the containing view, to calculate when the transition
     *          should start.
     */
    public static void updatePeekingCard(final NewTabPageRecyclerView recyclerView,
            final NewTabPageLayout ntpLayout, int viewportHeight) {
        // Get the first snippet that could display to make the peeking card transition.
        ViewGroup firstSnippet = (ViewGroup) getFirstViewMatchingViewType(recyclerView,
                NewTabPageListItem.VIEW_TYPE_SNIPPET);

        if (firstSnippet == null || !firstSnippet.isShown()) return;

        // If first snippet exists change the peeking card margin and padding to change its
        // width when scrolling.
        // Value used for max peeking card height and padding.
        int maxPadding = recyclerView.getResources().getDimensionPixelSize(
                R.dimen.snippets_padding_and_peeking_card_height);

        // The peeking card's resting position is |maxPadding| from the bottom of the screen hence
        // |viewportHeight - maxPadding|, and it grows the further it gets from this.
        // Also, make sure the |padding| is between 0 and |maxPadding|.
        final int padding =
                MathUtils.clamp(viewportHeight - maxPadding - firstSnippet.getTop(), 0, maxPadding);

        // Modify the padding so as the margin increases, the padding decreases, keeping the card's
        // contents in the same position. The top and bottom remain the same.
        firstSnippet.setPadding(padding, maxPadding, padding, maxPadding);

        RecyclerView.LayoutParams params =
                (RecyclerView.LayoutParams) firstSnippet.getLayoutParams();
        params.leftMargin = maxPadding - padding;
        params.rightMargin = maxPadding - padding;

        // Set the opacity of the card content to be 0 when peeking and 1 when full width.
        int firstSnippetChildCount = firstSnippet.getChildCount();
        for (int i = 0; i < firstSnippetChildCount; ++i) {
            View snippetChild = firstSnippet.getChildAt(i);
            snippetChild.setAlpha(padding / (float) maxPadding);
        }

        // If the peeking card is peeking, set its onClick to scroll to the articles section.
        // If not, reset its onClick to its ViewHolder.
        if (padding < maxPadding) {
            firstSnippet.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    // Offset the target scroll by the height of the omnibox (the top padding).
                    final int targetScroll = ntpLayout.getHeight() - ntpLayout.getPaddingTop();
                    // If (somehow) the peeking card is tapped while midway through the transition,
                    // we need to account for how much we have already scrolled.
                    recyclerView.smoothScrollBy(0, targetScroll - padding);
                }
            });
        } else {
            SnippetArticleViewHolder vh =
                    (SnippetArticleViewHolder) recyclerView.findContainingViewHolder(firstSnippet);
            firstSnippet.setOnClickListener(vh);
        }
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
        // Get the snippet header view.
        View snippetHeader = getFirstViewMatchingViewType(recyclerView,
                NewTabPageListItem.VIEW_TYPE_HEADER);

        if (snippetHeader == null || !snippetHeader.isShown()) return;

        // Start doing the calculations if the snippet header is currently shown on screen.
        RecyclerView.LayoutParams params =
                (RecyclerView.LayoutParams) snippetHeader.getLayoutParams();
        float headerAlpha = 0;
        int headerHeight = 0;

        // Get the max snippet header height.
        int maxSnippetHeaderHeight = recyclerView.getResources()
                .getDimensionPixelSize(R.dimen.snippets_article_header_height);
        // Measurement used to multiply the max snippet height to get a range on when to start
        // modifying the display of article header.
        final int numberHeaderHeight = 2;
        // Used to indicate when to start modifying the snippet header.
        int heightToStartChangingHeader = maxSnippetHeaderHeight * numberHeaderHeight;
        int snippetHeaderTop = snippetHeader.getTop();

        // Check if snippet header top is within range to start showing the snippet header.
        if (snippetHeaderTop < omniBoxHeight + heightToStartChangingHeader) {
            // The amount of space the article header has scrolled into the
            // |heightToStartChangingHeader|.
            int amountScrolledIntoHeaderSpace =
                    heightToStartChangingHeader - (snippetHeaderTop - omniBoxHeight);

            // Remove the |numberHeaderHeight| to get the actual header height we want to
            // display. Never let the height be more than the |maxSnippetHeaderHeight|.
            headerHeight = Math.min(
                    amountScrolledIntoHeaderSpace / numberHeaderHeight, maxSnippetHeaderHeight);

            // Get the alpha for the snippet header.
            headerAlpha = (float) headerHeight / maxSnippetHeaderHeight;
        }
        snippetHeader.setAlpha(headerAlpha);
        params.height = headerHeight;
        snippetHeader.setLayoutParams(params);

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
        int adapterSize = recyclerView.getAdapter().getItemCount();
        for (int i = 0; i < adapterSize; i++) {
            if (recyclerView.getAdapter().getItemViewType(i) == newTabPageListItemViewType) {
                return recyclerView.getLayoutManager().findViewByPosition(i);
            }
        }
        return null;
    }
}

