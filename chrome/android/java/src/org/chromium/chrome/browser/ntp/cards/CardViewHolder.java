// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.support.v4.view.animation.FastOutLinearInInterpolator;
import android.support.v4.view.animation.FastOutSlowInInterpolator;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.animation.Interpolator;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.UiConfig;
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
 *   {@link NewTabPageRecyclerView#scrollToFirstCard(int)}). Tap events in non-peeking state will be
 *   routed through {@link #onCardTapped()} for subclasses to override.
 *
 * - Cards will get some lateral margins when the viewport is sufficiently wide.
 *   (see {@link UiConfig#DISPLAY_STYLE_WIDE})
 *
 * Note: If a subclass overrides {@link #onBindViewHolder(NewTabPageListItem)}, it should call the
 * parent implementation to reset the private state when a card is recycled.
 */
public class CardViewHolder extends NewTabPageViewHolder {
    private static final Interpolator FADE_INTERPOLATOR = new FastOutLinearInInterpolator();

    private static final Interpolator TRANSITION_INTERPOLATOR = new FastOutSlowInInterpolator();

    /** Value used for max peeking card height and padding. */
    private final int mMaxPeekPadding;

    /**
     * Due to the card background being a 9 patch file - the card border shadow will be part of
     * the card width and height. This value will be used to adjust values to account for the
     * borders.
     */
    private final int mCards9PatchAdjustment;

    private final NewTabPageRecyclerView mRecyclerView;

    private final UiConfig mUiConfig;
    private final int mWideMarginSizePixels;
    private final DisplayStyleObserverAdapter mDisplayStyleObserverAdapter;

    /**
     * To what extent the card is "peeking". 0 means the card is not peeking at all and spans the
     * full width of its parent. 1 means it is fully peeking and will be shown with a margin.
     */
    private float mPeekingPercentage;

    /**
     * @param layoutId resource id of the layout to inflate and to use as card.
     * @param recyclerView ViewGroup that will contain the newly created view.
     * @param uiConfig The NTP UI configuration object used to adjust the card UI.
     */
    public CardViewHolder(
            int layoutId, final NewTabPageRecyclerView recyclerView, UiConfig uiConfig) {
        super(inflateView(layoutId, recyclerView));
        mWideMarginSizePixels = itemView.getResources().getDimensionPixelSize(
                R.dimen.ntp_wide_card_lateral_margins);

        mCards9PatchAdjustment = recyclerView.getResources().getDimensionPixelSize(
                R.dimen.snippets_card_9_patch_adjustment);

        mMaxPeekPadding = recyclerView.getResources().getDimensionPixelSize(
                R.dimen.snippets_padding_and_peeking_card_height);

        mRecyclerView = recyclerView;

        itemView.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (isPeeking()) {
                    recyclerView.scrollToFirstCard();
                } else {
                    onCardTapped();
                }
            }
        });

        mUiConfig = uiConfig;
        mDisplayStyleObserverAdapter = MarginResizer.createWithViewAdapter(itemView, mUiConfig);
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
        setPeekingPercentage(0f);

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
     */
    public void updatePeek() {
        float peekingPercentage;
        if (canPeek()) {
            // How much of the (top of the) card is visible?
            int visible = mRecyclerView.getHeight() - itemView.getTop();

            // If 1 padding unit (|mMaxPeekPadding|) is visible, the card is fully peeking. This is
            // reduced as the card is scrolled up, until 2 padding units are visible and the card is
            // not peeking anymore at all. Anything not between 0 and 1 is clamped.
            peekingPercentage = MathUtils.clamp(2f - (float) visible / mMaxPeekPadding, 0f, 1f);
        } else {
            peekingPercentage = 0f;
        }
        setPeekingPercentage(peekingPercentage);
    }

    /**
     * @return Whether the card is peeking.
     */
    public boolean isPeeking() {
        return mPeekingPercentage > 0f;
    }

    /**
     * @return Whether the card can peek.
     */
    public boolean canPeek() {
        // Only allow the card to peek if the user has not scrolled on the page beyond the
        // |mMaxPeekPadding| height. There will only ever be one peeking card on the page and only
        // when there is enough space to show the peeking card on the first screen at the bottom.
        return mRecyclerView.computeVerticalScrollOffset() <= itemView.getHeight();
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

    private void setPeekingPercentage(float peekingPercentage) {
        if (mPeekingPercentage == peekingPercentage) return;

        mPeekingPercentage = peekingPercentage;

        int peekPadding = (int) (mMaxPeekPadding
                * TRANSITION_INTERPOLATOR.getInterpolation(1f - peekingPercentage));

        // Modify the padding so as the margin increases, the padding decreases, keeping the card's
        // contents in the same position. The top and bottom remain the same.
        int lateralPadding;
        int lateralMargin;
        if (mDisplayStyleObserverAdapter.getDisplayStyle() != UiConfig.DISPLAY_STYLE_WIDE) {
            lateralPadding = peekPadding;

            // Adjust the padding by |mCards9PatchAdjustment| so the card width
            // is the actual width not including the elevation shadow, so we can have full bleed.
            lateralMargin = mMaxPeekPadding - (peekPadding + mCards9PatchAdjustment);
        } else {
            lateralPadding = mMaxPeekPadding;
            lateralMargin = mWideMarginSizePixels;
        }

        itemView.setPadding(lateralPadding, mMaxPeekPadding, lateralPadding, mMaxPeekPadding);

        RecyclerView.LayoutParams params = getParams();
        params.leftMargin = lateralMargin;
        params.rightMargin = lateralMargin;

        // Set the opacity of the card content to be 0 when peeking and 1 when full width.
        int itemViewChildCount = ((ViewGroup) itemView).getChildCount();
        for (int i = 0; i < itemViewChildCount; ++i) {
            View snippetChild = ((ViewGroup) itemView).getChildAt(i);
            snippetChild.setAlpha(peekPadding / (float) mMaxPeekPadding);
        }
    }

    private static View inflateView(int resourceId, ViewGroup parent) {
        return LayoutInflater.from(parent.getContext()).inflate(resourceId, parent, false);
    }

    private RecyclerView.LayoutParams getParams() {
        return (RecyclerView.LayoutParams) itemView.getLayoutParams();
    }
}
