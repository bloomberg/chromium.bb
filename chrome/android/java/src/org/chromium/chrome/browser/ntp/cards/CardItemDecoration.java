// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.content.Context;
import android.graphics.Rect;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.chromium.chrome.R;

/**
 * A decorator that places separators between RecyclerView cards.
 */
public class CardItemDecoration extends RecyclerView.ItemDecoration {
    private final int mCardsVerticalSpacing;

    /**
     * @param context used to retrieve resources.
     */
    public CardItemDecoration(Context context) {
        mCardsVerticalSpacing =
                context.getResources().getDimensionPixelSize(R.dimen.snippets_vertical_spacing);
    }

    @Override
    public void getItemOffsets(
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        int childPos = parent.getChildAdapterPosition(view);

        // childPos can be NO_POSITION if the method is called with a view being removed from the
        // RecyclerView, for example when dismissing an item.
        if (childPos == RecyclerView.NO_POSITION) return;

        // Add spacing if the current item and the next are cards.
        if (parent.getChildViewHolder(view) instanceof CardViewHolder
                && parent.findViewHolderForAdapterPosition(childPos + 1)
                                instanceof CardViewHolder) {
            outRect.set(0, 0, 0, mCardsVerticalSpacing);
        }
    }
}