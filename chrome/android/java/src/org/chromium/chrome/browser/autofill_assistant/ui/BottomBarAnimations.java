// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.ui;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.support.design.widget.CoordinatorLayout;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.compositor.layouts.ChromeAnimation;

/**
 * Wrapper around the different {@code AutofillAssistantUiDelegate} views to animate between states.
 */
public class BottomBarAnimations {
    private static final int DETAILS_SLIDE_ANIMATION_SPEED_MS = 200;
    private static final int CAROUSEL_FADE_ANIMATION_SPEED_MS = 100;

    private static final int BOTTOM_BAR_WITH_DETAILS_HEIGHT_DP = 234;
    private static final int DETAILS_HORIZONTAL_MARGIN_DP = 24;
    private static final int DETAILS_VERTICAL_MARGIN_WITH_CAROUSEL_DP = 20;
    private static final int DETAILS_VERTICAL_MARGIN_WITHOUT_CAROUSEL_DP = 46;
    private static final int CAROUSEL_TOP_PADDING_WITHOUT_DETAILS_DP = 16;

    private final View mBottomBarView;
    private final View mDetailsView;
    private final View mCarouselView;

    // Dimensions in device pixels.
    private final int mBottomBarWithDetailsHeight;
    private final int mDetailsHorizontalMargin;
    private final int mDetailsVerticalMarginWithCarousel;
    private final int mDetailsVerticalMarginWithoutCarousel;
    private final int mCarouselTopPaddingWithoutDetails;

    private ValueAnimator mCurrentDetailsAnimation;

    public BottomBarAnimations(View bottomBarView, View detailsView, View carouselView,
            DisplayMetrics displayMetrics) {
        mBottomBarView = bottomBarView;
        mDetailsView = detailsView;
        mCarouselView = carouselView;

        mBottomBarWithDetailsHeight = (int) TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, BOTTOM_BAR_WITH_DETAILS_HEIGHT_DP, displayMetrics);
        mDetailsHorizontalMargin = (int) TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, DETAILS_HORIZONTAL_MARGIN_DP, displayMetrics);
        mDetailsVerticalMarginWithCarousel =
                (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                        DETAILS_VERTICAL_MARGIN_WITH_CAROUSEL_DP, displayMetrics);
        mDetailsVerticalMarginWithoutCarousel =
                (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                        DETAILS_VERTICAL_MARGIN_WITHOUT_CAROUSEL_DP, displayMetrics);
        mCarouselTopPaddingWithoutDetails =
                (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                        CAROUSEL_TOP_PADDING_WITHOUT_DETAILS_DP, displayMetrics);

        // Initialize the margins and paddings.
        setCarouselPaddings();
        setDetailsMargins(mDetailsVerticalMarginWithoutCarousel);
    }

    /**
     * Show the carousel. This method does nothing if the carousel is already shown.
     */
    public void showCarousel() {
        if (mCarouselView.getVisibility() == View.VISIBLE) {
            return;
        }

        mCarouselView.setVisibility(View.VISIBLE);

        if (mDetailsView.getVisibility() != View.VISIBLE) {
            animateCarouselAlpha(1.0f, ignoredResult -> {});
            return;
        }

        // Slide details to the top.
        animateDetailsVerticalMargin(mDetailsVerticalMarginWithCarousel, (success) -> {
            // If details animation was cancelled, we don't do anything.
            if (success) {
                animateCarouselAlpha(1.0f, ignoredResult -> {});
            }
        });
    }

    /**
     * Show the carousel. This method does nothing if the carousel is already hidden.
     */
    public void hideCarousel() {
        if (mCarouselView.getVisibility() != View.VISIBLE) {
            return;
        }

        animateCarouselAlpha(0f, (success) -> {
            // Slide details to the middle of the bottom bar if carousel animation was not
            // cancelled.
            if (success) {
                mCarouselView.setVisibility(View.GONE);

                if (mDetailsView.getVisibility() == View.VISIBLE) {
                    animateDetailsVerticalMargin(
                            mDetailsVerticalMarginWithoutCarousel, ignoredResult -> {});
                }
            }
        });
    }

    /**
     * Show the details. This method does nothing if the details are already shown.
     */
    public void showDetails() {
        if (mDetailsView.getVisibility() == View.VISIBLE) {
            return;
        }

        // TODO(crbug.com/806868): The bottom bar will be animated when its height changes, but
        // maybe we want to also animate the details appearance. In practice, most of the time the
        // details are shown when running a script, i.e. when no chips is currently shown, which
        // means the details will slide alongside the bottom bar.
        mDetailsView.setVisibility(View.VISIBLE);
        setBottomBarHeight(mBottomBarWithDetailsHeight);
        setCarouselPaddings();
    }

    /** Update the bottom bar height to wrap when the details are shown.. */
    public void setBottomBarHeightToWrapContent() {
        if (mDetailsView.getVisibility() == View.VISIBLE) {
            setBottomBarHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        }
    }

    /** Update the bottom bar height to fixed when the details are shown. */
    public void setBottomBarHeightToFixed() {
        if (mDetailsView.getVisibility() == View.VISIBLE) {
            setBottomBarHeight(mBottomBarWithDetailsHeight);
        }
    }

    /**
     * Show the details. This method does nothing if the details are already hidden.
     */
    public void hideDetails() {
        if (mDetailsView.getVisibility() != View.VISIBLE) {
            return;
        }

        mDetailsView.setVisibility(View.GONE);
        setCarouselPaddings();
        setBottomBarHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private void setBottomBarHeight(int targetHeight) {
        CoordinatorLayout.LayoutParams layoutParams =
                (CoordinatorLayout.LayoutParams) mBottomBarView.getLayoutParams();
        layoutParams.height = targetHeight;
        mBottomBarView.setLayoutParams(layoutParams);
    }

    /**
     * Animate the carousel opacity from current value to {@code targetOpacity}. Calls {@code
     * onAnimationEnd} with true if the animation finished successfully, false if it was cancelled.
     */
    private void animateCarouselAlpha(float targetOpacity, Callback<Boolean> onAnimationEnd) {
        float currentOpacity = mCarouselView.getAlpha();
        mCarouselView.animate()
                .alpha(targetOpacity)
                .setDuration((int) (CAROUSEL_FADE_ANIMATION_SPEED_MS
                        * Math.abs(targetOpacity - currentOpacity)))
                .setListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        onAnimationEnd.onResult(true);
                    }

                    @Override
                    public void onAnimationCancel(Animator animation) {
                        onAnimationEnd.onResult(false);
                    }
                })
                .setInterpolator(ChromeAnimation.getLinearInterpolator())
                .start();
    }

    /**
     * Animate the details vertical margin from current value to {@code targetMargin}. Calls {@code
     * onAnimationEnd} with true if the animation finished successfully, false if it was cancelled.
     */
    private void animateDetailsVerticalMargin(
            int targetVerticalMargin, Callback<Boolean> onAnimationEnd) {
        if (mCurrentDetailsAnimation != null) {
            mCurrentDetailsAnimation.cancel();
        }

        int currentVerticalMargin =
                ((LinearLayout.LayoutParams) mDetailsView.getLayoutParams()).topMargin;

        mCurrentDetailsAnimation =
                ValueAnimator.ofInt(currentVerticalMargin, targetVerticalMargin)
                        .setDuration(DETAILS_SLIDE_ANIMATION_SPEED_MS
                                * Math.abs(currentVerticalMargin - targetVerticalMargin)
                                / (mDetailsVerticalMarginWithoutCarousel
                                          - mDetailsVerticalMarginWithCarousel));
        mCurrentDetailsAnimation.setInterpolator(ChromeAnimation.getDecelerateInterpolator());
        mCurrentDetailsAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation, boolean isReverse) {
                mCurrentDetailsAnimation = null;
                onAnimationEnd.onResult(true);
            }

            @Override
            public void onAnimationCancel(Animator animation) {
                mCurrentDetailsAnimation = null;
                onAnimationEnd.onResult(false);
            }
        });
        mCurrentDetailsAnimation.addUpdateListener(
                animation -> setDetailsMargins((int) animation.getAnimatedValue()));
        mCurrentDetailsAnimation.start();
    }

    private void setDetailsMargins(int verticalMargin) {
        LinearLayout.LayoutParams layoutParams =
                (LinearLayout.LayoutParams) mDetailsView.getLayoutParams();
        layoutParams.setMargins(
                mDetailsHorizontalMargin, verticalMargin, mDetailsHorizontalMargin, verticalMargin);
        mDetailsView.setLayoutParams(layoutParams);
    }

    private void setCarouselPaddings() {
        int topPadding = 0;
        if (mDetailsView.getVisibility() != View.VISIBLE) {
            topPadding = mCarouselTopPaddingWithoutDetails;
        }
        mCarouselView.setPadding(mCarouselView.getPaddingLeft(), topPadding,
                mCarouselView.getPaddingRight(), mCarouselView.getPaddingBottom());
    }
}
