// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.support.v4.view.animation.FastOutLinearInInterpolator;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.animation.Interpolator;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.util.MathUtils;

/**
 * Holder for a generic card.
 *
 * Specific behaviors added to the cards:
 *
 * - Cards can shrink and fade their appearance so that they can be made peeking above the screen
 *   limit.
 *
 * - When peeking, tapping on cards will make them request a scroll up (see
 *   {@link NewTabPageRecyclerView#showCardsFrom(int)}). Tap events in non-peeking state will be
 *   routed through {@link #onCardTapped()} for subclasses to override.
 *
 * Note: If a subclass overrides {@link #onBindViewHolder(NewTabPageListItem)}, it should call the
 * parent implementation to reset the private state when a card is recycled.
 */
public class CardViewHolder extends NewTabPageViewHolder {
    private static final Interpolator FADE_INTERPOLATOR = new FastOutLinearInInterpolator();

    /** Value used for max peeking card height and padding. */
    private final int mMaxPeekPadding;

    /**
     * Due to the card background being a 9 patch file - the card border shadow will be part of
     * the card width and height. This value will be used to adjust values to account for the
     * borders.
     */
    private final int mCards9PatchAdjustment;

    private final NewTabPageRecyclerView mRecyclerView;

    /**
     * Current padding value. The padding and the margins are manipulated together to create the
     * shrunk/peeking appearance of the cards. When the padding is low, the margins are high and
     * the background is visible around the cards. When the padding is maximum
     * (see {{@link #mMaxPeekPadding}}), the card spans from an edge of the parent to the other,
     * and is not peeking anymore.
     */
    private int mPeekPadding;

    /**
     * @param layoutId resource id of the layout to inflate and to use as card.
     * @param recyclerView ViewGroup that will contain the newly created view.
     */
    public CardViewHolder(int layoutId, final NewTabPageRecyclerView recyclerView) {
        super(inflateView(layoutId, recyclerView));

        mCards9PatchAdjustment = recyclerView.getResources().getDimensionPixelSize(
                R.dimen.snippets_card_9_patch_adjustment);

        mMaxPeekPadding = recyclerView.getResources().getDimensionPixelSize(
                R.dimen.snippets_padding_and_peeking_card_height);
        mPeekPadding = mMaxPeekPadding;

        mRecyclerView = recyclerView;

        itemView.setOnClickListener(new OnClickListener() {

            @Override
            public void onClick(View v) {
                if (isPeeking()) {
                    recyclerView.showCardsFrom(mPeekPadding);
                } else {
                    onCardTapped();
                }
            }
        });
    }

    /**
     * Called when the NTP cards adapter is requested to update the currently visible ViewHolder
     * with data. {@link CardViewHolder}'s implementation must be called by subclasses.
     *
     * @param item The NewTabPageListItem object that holds the data for this ViewHolder
     */
    @Override
    public void onBindViewHolder(NewTabPageListItem item) {
        // Reset the peek status to avoid recycled view holders to be peeking at the wrong moment.
        setPeekingStateForPadding(mMaxPeekPadding);

        // Reset the transparency in case a faded card is being recycled.
        itemView.setAlpha(1f);
    }

    @Override
    public void updateLayoutParams() {
        RecyclerView.LayoutParams params = getParams();

        // Each card has the full elevation effect so we will remove bottom margin to overlay and
        // hide the bottom shadow of the previous card to give the effect of a divider instead of a
        // shadow.
        if (mRecyclerView.getAdapter().getItemViewType(getAdapterPosition() + 1)
                == NewTabPageListItem.VIEW_TYPE_SNIPPET) {
            params.bottomMargin = -mCards9PatchAdjustment;
        } else {
            params.bottomMargin = 0;
        }
    }

    /**
     * Change the width, padding and child opacity of the card to give a smooth transition as the
     * user scrolls.
     * @param viewportHeight The height of the containing viewport, i.e. the area inside the
     *          containing view that is available for drawing.
     */
    public void updatePeek(int viewportHeight) {
        // The peeking card's resting position is |mMaxPeekPadding| from the bottom of the screen
        // hence |viewportHeight - mMaxPeekPadding|, and it grows the further it gets from this.
        // Also, make sure the |padding| is between 0 and |mMaxPeekPadding|.
        setPeekingStateForPadding(MathUtils.clamp(
                viewportHeight - mMaxPeekPadding - itemView.getTop(), 0, mMaxPeekPadding));
    }

    /** Returns whether the card is currently peeking. */
    public boolean isPeeking() {
        return mPeekPadding < mMaxPeekPadding;
    }

    @Override
    public void updateViewStateForDismiss(float dX) {
        float input = Math.abs(dX) / itemView.getMeasuredWidth();
        float alpha = 1 - FADE_INTERPOLATOR.getInterpolation(input);
        itemView.setAlpha(alpha);
    }

    /**
     * Override this to react when the card is tapped. This method will not be called if the card is
     * currently peeking.
     */
    protected void onCardTapped() {}

    private void setPeekingStateForPadding(int padding) {
        // TODO(mcwilliams): Change the 'padding' value to a percentage of what the transition
        // should be which can be used for alpha and bleed.
        mPeekPadding = padding;

        // Modify the padding so as the margin increases, the padding decreases, keeping the card's
        // contents in the same position. The top and bottom remain the same.
        itemView.setPadding(mPeekPadding, mMaxPeekPadding, mPeekPadding, mMaxPeekPadding);

        // This mCards9PatchAdjustment value will be used to adjust the padding so the card width
        // is the actual width not including the elevation shadow so we can have full bleed.
        RecyclerView.LayoutParams params = getParams();
        params.leftMargin = mMaxPeekPadding - (mPeekPadding + mCards9PatchAdjustment);
        params.rightMargin = mMaxPeekPadding - (mPeekPadding + mCards9PatchAdjustment);

        // Set the opacity of the card content to be 0 when peeking and 1 when full width.
        int itemViewChildCount = ((ViewGroup) itemView).getChildCount();
        for (int i = 0; i < itemViewChildCount; ++i) {
            View snippetChild = ((ViewGroup) itemView).getChildAt(i);
            snippetChild.setAlpha(mPeekPadding / (float) mMaxPeekPadding);
        }
    }

    private static View inflateView(int resourceId, ViewGroup parent) {
        return LayoutInflater.from(parent.getContext()).inflate(resourceId, parent, false);
    }

    private RecyclerView.LayoutParams getParams() {
        return (RecyclerView.LayoutParams) itemView.getLayoutParams();
    }
}
