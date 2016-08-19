// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.support.v4.view.animation.FastOutLinearInInterpolator;
import android.support.v4.view.animation.FastOutSlowInInterpolator;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.animation.Interpolator;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
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
 * Note: If a subclass overrides {@link #onBindViewHolder(NewTabPageItem)}, it should call the
 * parent implementation to reset the private state when a card is recycled.
 */
public class CardViewHolder extends NewTabPageViewHolder {
    private static final Interpolator FADE_INTERPOLATOR = new FastOutLinearInInterpolator();
    private static final Interpolator TRANSITION_INTERPOLATOR = new FastOutSlowInInterpolator();
    private static final int DISMISS_ANIMATION_TIME_MS = 300;

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
    private final MarginResizer mMarginResizer;

    /**
     * To what extent the card is "peeking". 0 means the card is not peeking at all and spans the
     * full width of its parent. 1 means it is fully peeking and will be shown with a margin.
     */
    private float mPeekingPercentage;

    private boolean mCanPeek = false;

    /**
     * @param layoutId resource id of the layout to inflate and to use as card.
     * @param recyclerView ViewGroup that will contain the newly created view.
     * @param uiConfig The NTP UI configuration object used to adjust the card UI.
     */
    public CardViewHolder(
            int layoutId, final NewTabPageRecyclerView recyclerView, UiConfig uiConfig) {
        super(inflateView(layoutId, recyclerView));

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

        itemView.setOnCreateContextMenuListener(new View.OnCreateContextMenuListener() {
            @Override
            public void onCreateContextMenu(ContextMenu menu, View view, ContextMenuInfo menuInfo) {
                if (!isPeeking()) {
                    CardViewHolder.this.createContextMenu(menu);
                }
            }
        });

        mUiConfig = uiConfig;

        mMarginResizer = MarginResizer.createWithViewAdapter(itemView, mUiConfig);

        // Configure the resizer to use negative margins on regular display to balance out the
        // lateral shadow of the card 9-patch and avoid a rounded corner effect.
        mMarginResizer.setMargins(-mCards9PatchAdjustment);
    }

    /**
     * Called when the NTP cards adapter is requested to update the currently visible ViewHolder
     * with data. {@link CardViewHolder}'s implementation must be called by subclasses.
     *
     * @param item The NewTabPageListItem object that holds the data for this ViewHolder
     */
    @Override
    public void onBindViewHolder(NewTabPageItem item) {
        // Reset the peek status to avoid recycled view holders to be peeking at the wrong moment.
        int firstCardPos = mRecyclerView.getNewTabPageAdapter().getFirstCardPosition();
        NewTabPageLayout aboveTheFold = mRecyclerView.findAboveTheFoldView();
        setCanPeek(getAdapterPosition() == firstCardPos
                && aboveTheFold != null && aboveTheFold.hasSpaceForPeekingCard());
        updatePeek();

        // Reset the transparency and translation in case a dismissed card is being recycled.
        itemView.setAlpha(1f);
        itemView.setTranslationX(0f);
    }

    @Override
    public void updateLayoutParams() {
        // Each card has the full elevation effect so we will remove bottom margin to overlay and
        // hide the bottom shadow of the previous card to give the effect of a divider instead of a
        // shadow.
        // Notes:
        //  - An alternative way to check that 2 cards are coming one after the other would be to
        //    check the item's ViewType, but we decided against for now to avoid having to maintain
        //    a separate listing of all the view types that are cards. Their ViewHolders are all
        //    CardViewHolder so it's a robust way to do the check.
        //  - The drawback of using ViewHolders is that they are not instantiated yet for all the
        //    items in the list. At a given moment, the below code will not properly adjust the
        //    margin of cards for which the next card ViewHolder has not yet been created and added
        //    to the RecyclerView. It doesn't matter still because a layout pass will be requested
        //    when that ViewHolder is added, calling this method again and fixing the margins
        //    before they enter the view.
        ViewHolder previousViewHolder =
                mRecyclerView.findViewHolderForAdapterPosition(getAdapterPosition() - 1);
        if (previousViewHolder instanceof CardViewHolder) {
            ((CardViewHolder) previousViewHolder).getParams().bottomMargin =
                    -mCards9PatchAdjustment;
        }

        // Reset the margin to 0 here. If it should take the 9-patch adjustment into account, the
        // next card will set the value, since the method is called on the cards in layout order.
        getParams().bottomMargin = 0;
    }

    /**
     * Change the width, padding and child opacity of the card to give a smooth transition as the
     * user scrolls.
     */
    public void updatePeek() {
        float peekingPercentage;
        if (getCanPeek()) {
            // Find the position of the top of the current card. We use the bottom of the previous
            // view for it because near initialization time, the view has not been placed yet in
            // the recycler view and we get a wrong value.
            ViewHolder previousViewHolder =
                    mRecyclerView.findViewHolderForAdapterPosition(getAdapterPosition() - 1);
            int top = previousViewHolder == null ? itemView.getTop()
                                                : previousViewHolder.itemView.getBottom();

            // How much of the (top of the) card is visible?
            int visible = mRecyclerView.getHeight() - top;

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
     * Set whether the card can peek.
     */
    public void setCanPeek(boolean canPeek) {
        mCanPeek = canPeek;
    }

    /**
     * @return Whether the card can peek.
     */
    public boolean getCanPeek() {
        return mCanPeek;
    }

    @Override
    public void updateViewStateForDismiss(float dX) {
        // Any changes in the animation here should be reflected also in |dismiss|
        // and reset in onBindViewHolder.
        float input = Math.abs(dX) / itemView.getMeasuredWidth();
        float alpha = 1 - FADE_INTERPOLATOR.getInterpolation(input);
        itemView.setAlpha(alpha);
    }

    /**
     * Override this to react when the card is tapped. This method will not be called if the card is
     * currently peeking.
     */
    protected void onCardTapped() {}

    /**
     * Override this to provide a context menu for the card. This method will not be called if the
     * card is currently peeking.
     */
    protected void createContextMenu(ContextMenu menu) {}

    private void setPeekingPercentage(float peekingPercentage) {
        if (mPeekingPercentage == peekingPercentage) return;

        mPeekingPercentage = peekingPercentage;

        int peekPadding = (int) (mMaxPeekPadding
                * TRANSITION_INTERPOLATOR.getInterpolation(1f - peekingPercentage));

        // Modify the padding so as the margin increases, the padding decreases, keeping the card's
        // contents in the same position. The top and bottom remain the same.
        int lateralPadding;
        if (mUiConfig.getCurrentDisplayStyle() != UiConfig.DISPLAY_STYLE_WIDE) {
            lateralPadding = peekPadding;
        } else {
            lateralPadding = mMaxPeekPadding;
        }
        itemView.setPadding(lateralPadding, mMaxPeekPadding, lateralPadding, mMaxPeekPadding);

        // Adjust the margins by |mCards9PatchAdjustment| so the card width
        // is the actual width not including the elevation shadow, so we can have full bleed.
        mMarginResizer.setMargins(mMaxPeekPadding - (peekPadding + mCards9PatchAdjustment));

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

    /**
     * Animates the card being swiped to the right as if the user had dismissed it. You must check
     * {@link #isDismissable()} before calling.
     */
    protected void dismiss() {
        assert isDismissable();

        // In case the user pressed dismiss on the context menu after swiping to dismiss.
        if (getAdapterPosition() == RecyclerView.NO_POSITION) return;

        // Any changes in the animation here should be reflected also in |updateViewStateForDismiss|
        // and reset in onBindViewHolder.
        AnimatorSet animation = new AnimatorSet();
        animation.playTogether(
                ObjectAnimator.ofFloat(itemView, View.ALPHA, 0f),
                ObjectAnimator.ofFloat(itemView, View.TRANSLATION_X, (float) itemView.getWidth()));

        animation.setDuration(DISMISS_ANIMATION_TIME_MS);
        animation.setInterpolator(FADE_INTERPOLATOR);
        animation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mRecyclerView.onItemDismissStarted(itemView);
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                ((NewTabPageAdapter) mRecyclerView.getAdapter()).dismissItem(CardViewHolder.this);
                mRecyclerView.onItemDismissFinished(itemView);
            }
        });
        animation.start();
    }
}
