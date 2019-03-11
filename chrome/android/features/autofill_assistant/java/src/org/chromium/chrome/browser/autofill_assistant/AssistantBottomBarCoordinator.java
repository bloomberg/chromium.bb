// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.support.design.widget.BottomSheetBehavior;
import android.support.design.widget.CoordinatorLayout;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantCarouselCoordinator;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetailsCoordinator;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderCoordinator;
import org.chromium.chrome.browser.autofill_assistant.infobox.AssistantInfoBoxCoordinator;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantPaymentRequestCoordinator;

/**
 * Coordinator responsible for the Autofill Assistant bottom bar. This coordinator allows to enable
 * or disable the swipeable behavior of the bottom bar and ensures that the bottom bar height is
 * constant during the script execution (if possible) by adapting the spacing between its child
 * views (details, infobox, payment request and carousel).
 */
class AssistantBottomBarCoordinator {
    // The top padding that should be applied to the bottom bar when the swiping indicator is
    // hidden.
    private static final int BOTTOM_BAR_WITHOUT_INDICATOR_PADDING_TOP_DP = 16;

    private final ViewGroup mBottomBarView;
    private final ViewGroup mBottomBarContainerView;
    private final View mSwipeIndicatorView;
    private final BottomSheetBehavior mBottomBarBehavior;

    // Dimensions in device pixels.
    private final int mBottomBarWithoutIndicatorPaddingTop;

    // Child coordinators.
    private final AssistantHeaderCoordinator mHeaderCoordinator;
    private final AssistantInfoBoxCoordinator mInfoBoxCoordinator;
    private final AssistantDetailsCoordinator mDetailsCoordinator;
    private final AssistantPaymentRequestCoordinator mPaymentRequestCoordinator;
    private final AssistantCarouselCoordinator mSuggestionsCoordinator;
    private final AssistantCarouselCoordinator mActionsCoordinator;

    AssistantBottomBarCoordinator(Context context, View assistantView, AssistantModel model) {
        mBottomBarView = assistantView.findViewById(R.id.autofill_assistant_bottombar);
        mBottomBarContainerView =
                mBottomBarView.findViewById(R.id.autofill_assistant_bottombar_container);
        mSwipeIndicatorView = mBottomBarView.findViewById(R.id.swipe_indicator);
        mBottomBarBehavior = BottomSheetBehavior.from(mBottomBarView);

        DisplayMetrics displayMetrics = context.getResources().getDisplayMetrics();
        mBottomBarWithoutIndicatorPaddingTop =
                (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                        BOTTOM_BAR_WITHOUT_INDICATOR_PADDING_TOP_DP, displayMetrics);

        // Instantiate child components.
        mHeaderCoordinator =
                new AssistantHeaderCoordinator(context, mBottomBarView, model.getHeaderModel());
        mInfoBoxCoordinator = new AssistantInfoBoxCoordinator(context, model.getInfoBoxModel());
        mDetailsCoordinator = new AssistantDetailsCoordinator(context, model.getDetailsModel());
        mPaymentRequestCoordinator =
                new AssistantPaymentRequestCoordinator(context, model.getPaymentRequestModel());
        mSuggestionsCoordinator =
                new AssistantCarouselCoordinator(context, model.getSuggestionsModel());
        mActionsCoordinator = new AssistantCarouselCoordinator(context, model.getActionsModel());

        // Add child views to bottom bar container.
        mBottomBarContainerView.addView(mInfoBoxCoordinator.getView());
        mBottomBarContainerView.addView(mDetailsCoordinator.getView());
        mBottomBarContainerView.addView(mPaymentRequestCoordinator.getView());
        mBottomBarContainerView.addView(mSuggestionsCoordinator.getView());
        mBottomBarContainerView.addView(mActionsCoordinator.getView());

        // We set the horizontal margins of the details and payment request. We don't set a padding
        // to the container as we want the carousels children to be scrolled at the limit of the
        // screen.
        setHorizontalMargins(mInfoBoxCoordinator.getView());
        setHorizontalMargins(mDetailsCoordinator.getView());
        setHorizontalMargins(mPaymentRequestCoordinator.getView());
    }

    /**
     * Return the container view representing the bottom bar. Adding child views to this view should
     * add them below the header.
     */
    public ViewGroup getView() {
        return mBottomBarView;
    }

    /**
     * Returns the view container inside the bottom bar view.
     */
    public ViewGroup getContainerView() {
        return mBottomBarContainerView;
    }

    /**
     * Make sure the bottom bar is expanded and text is visible.
     */
    public void expand() {
        mBottomBarBehavior.setState(BottomSheetBehavior.STATE_EXPANDED);
    }

    /**
     * Enable or disable to swipeable behavior of the bottom bar.
     */
    public void allowSwipingBottomSheet(boolean allowed) {
        CoordinatorLayout.LayoutParams params =
                (CoordinatorLayout.LayoutParams) mBottomBarView.getLayoutParams();
        if (allowed) {
            params.setBehavior(mBottomBarBehavior);
            mSwipeIndicatorView.setVisibility(View.VISIBLE);
            setBottomBarPaddingTop(0);
        } else {
            params.setBehavior(null);
            mSwipeIndicatorView.setVisibility(View.GONE);
            setBottomBarPaddingTop(mBottomBarWithoutIndicatorPaddingTop);
        }
    }

    @VisibleForTesting
    public AssistantCarouselCoordinator getSuggestionsCoordinator() {
        return mSuggestionsCoordinator;
    }

    @VisibleForTesting
    public AssistantCarouselCoordinator getActionsCoordinator() {
        return mActionsCoordinator;
    }

    private void setBottomBarPaddingTop(int paddingPx) {
        mBottomBarView.setPadding(0, paddingPx, 0, 0);
    }

    private void setHorizontalMargins(View view) {
        LinearLayout.LayoutParams layoutParams = (LinearLayout.LayoutParams) view.getLayoutParams();
        int horizontalMargin = view.getContext().getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_bottombar_horizontal_spacing);
        layoutParams.setMarginStart(horizontalMargin);
        layoutParams.setMarginEnd(horizontalMargin);
        view.setLayoutParams(layoutParams);
    }
}
