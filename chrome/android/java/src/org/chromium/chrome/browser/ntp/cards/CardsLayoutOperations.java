// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import org.chromium.chrome.browser.ntp.MostVisitedLayout;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.NewTabPageUma.SnapState;


/**
 * This is a class to organise the various layout operations of the New Tab Page while using the
 * cards layout. For example, updating the size of the peeking card or controlling the scroll
 * behaviour.
 */
public class CardsLayoutOperations {

    /**
     * Snaps the scroll point to the top of the screen, the top of most visited or to articles
     * depending on the current scroll. This function makes gross assumptions about the layout, so
     * will probably need to be changed if the new tab page changes.
     * @param recyclerView The NewTabPageRecyclerView containing everything.
     * @param newTabPageLayout The above the fold content.
     * @param mostVisitedLayout The view to snap to while snapping to most visited.
     * @param currentScroll The current scroll.
     * @param forceMostLikelySnap Whether to force snapping to most likely.
     * @return Whether the snap point was above or below the fold.
     */
    public static NewTabPageUma.SnapState snapScroll(NewTabPageRecyclerView recyclerView,
            NewTabPageLayout newTabPageLayout, MostVisitedLayout mostVisitedLayout,
            int currentScroll, boolean forceMostLikelySnap) {
        // These calculations only work if the first item is visible (since
        // computeVerticalScrollOffset only takes into account visible items).
        // Luckily, we only need to perform the calculations if the first item is visible.
        if (!recyclerView.isFirstItemVisible()) {
            return SnapState.BELOW_THE_FOLD;
        }

        // If snapping to Most Likely or to Articles, the omnibox will be at the top of the
        // page, so offset the scroll so the scroll targets appear below it.
        final int omniBoxHeight = newTabPageLayout.getPaddingTop();
        final int topOfMostLikelyScroll = mostVisitedLayout.getTop() - omniBoxHeight;
        final int topOfSnippetsScroll = newTabPageLayout.getHeight() - omniBoxHeight;

        assert currentScroll >= 0;
        // Do not do any scrolling if the user is currently viewing articles.
        if (currentScroll >= topOfSnippetsScroll) {
            return SnapState.BELOW_THE_FOLD;
        }

        // If Most Likely is fully visible when we are scrolled to the top, we have two
        // snap points: the Top and Articles.
        // If not, we have three snap points: the Top, Most Likely and Articles.
        boolean snapToMostLikely =
                 forceMostLikelySnap || recyclerView.getHeight() < mostVisitedLayout.getBottom();

        int targetScroll;
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
        }
        recyclerView.smoothScrollBy(0, targetScroll - currentScroll);

        return targetScroll == topOfSnippetsScroll
                ? SnapState.BELOW_THE_FOLD : SnapState.ABOVE_THE_FOLD;
    }
}
