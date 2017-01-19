// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.graphics.Region;
import android.support.annotation.IntDef;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.Interpolator;
import android.widget.LinearLayout;

import org.chromium.chrome.browser.util.MathUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class defines the bottom sheet that has multiple states and a persistently showing toolbar.
 * Namely, the states are:
 * - PEEK: Only the toolbar is visible at the bottom of the screen.
 * - HALF: The sheet is expanded to consume around half of the screen.
 * - FULL: The sheet is expanded to its full height.
 *
 * All the computation in this file is based off of the bottom of the screen instead of the top
 * for simplicity. This means that the bottom of the screen is 0 on the Y axis.
 */
public class BottomSheet extends LinearLayout {
    /** The different states that the bottom sheet can have. */
    @IntDef({SHEET_STATE_PEEK, SHEET_STATE_HALF, SHEET_STATE_FULL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SheetState {}
    public static final int SHEET_STATE_PEEK = 0;
    public static final int SHEET_STATE_HALF = 1;
    public static final int SHEET_STATE_FULL = 2;

    /**
     * The base duration of the settling animation of the sheet. 218 ms is a spec for material
     * design (this is the minimum time a user is guaranteed to pay attention to something).
     */
    private static final long BASE_ANIMATION_DURATION_MS = 218;

    /**
     * The fraction of the way to the next state the sheet must be swiped to animate there when
     * released. A smaller value here means a smaller swipe is needed to move the sheet around.
     */
    private static final float THRESHOLD_TO_NEXT_STATE = 0.5f;

    /** The minimum y/x ratio that a scroll must have to be considered vertical. */
    private static final float MIN_VERTICAL_SCROLL_SLOPE = 2.0f;

    /**
     * Information about the different scroll states of the sheet. Order is important for these,
     * they go from smallest to largest.
     */
    private static final int[] sStates =
            new int[] {SHEET_STATE_PEEK, SHEET_STATE_HALF, SHEET_STATE_FULL};
    private final float[] mStateRatios = new float[] {0.0f, 0.55f, 0.95f};

    /** The interpolator that the height animator uses. */
    private final Interpolator mInterpolator = new DecelerateInterpolator(1.0f);

    /** For detecting scroll and fling events on the bottom sheet. */
    private GestureDetector mGestureDetector;

    /** Whether or not the user is scrolling the bottom sheet. */
    private boolean mIsScrolling;

    /** Track the velocity of the user's scrolls to determine up or down direction. */
    private VelocityTracker mVelocityTracker;

    /** The animator used to move the sheet to a fixed state when released by the user. */
    private ObjectAnimator mSettleAnimator;

    /** The height of the toolbar. */
    private float mToolbarHeight;

    /** The height of the view that contains the bottom sheet. */
    private float mContainerHeight;

    /** The current sheet state. If the sheet is moving, this will be the target state. */
    private int mCurrentState;

    /**
     * This class is responsible for detecting swipe and scroll events on the bottom sheet or
     * ignoring them when appropriate.
     */
    private class BottomSheetSwipeDetector extends GestureDetector.SimpleOnGestureListener {
        @Override
        public boolean onDown(MotionEvent e) {
            return true;
        }

        @Override
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX,
                float distanceY) {
            // Only start scrolling if the scroll is up or down. If the user is already scrolling,
            // continue moving the sheet.
            float slope = Math.abs(distanceX) > 0f ? Math.abs(distanceY) / Math.abs(distanceX) : 0f;
            if (!mIsScrolling && slope < MIN_VERTICAL_SCROLL_SLOPE) {
                mVelocityTracker.clear();
                return false;
            }

            // Cancel the settling animation if it is running so it doesn't conflict with where the
            // user wants to move the sheet.
            cancelAnimation();

            mVelocityTracker.addMovement(e2);

            float currentShownRatio =
                    mContainerHeight > 0 ? getSheetOffsetFromBottom() / mContainerHeight : 0;

            // If the sheet is in the max position, don't move if the scroll is upward.
            if (currentShownRatio >= mStateRatios[mStateRatios.length - 1]
                    && distanceY > 0) {
                mIsScrolling = false;
                return false;
            }

            // Similarly, if the sheet is in the min position, don't move if the scroll is downward.
            if (currentShownRatio <= mStateRatios[0] && distanceY < 0) {
                mIsScrolling = false;
                return false;
            }

            float newOffset = getSheetOffsetFromBottom() + distanceY;
            setSheetOffsetFromBottom(MathUtils.clamp(newOffset, getMinOffset(), getMaxOffset()));

            mIsScrolling = true;
            return true;
        }

        @Override
        public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX,
                float velocityY) {
            cancelAnimation();

            // Figure out the projected state of the sheet and animate there. Note that a swipe up
            // will have a negative velocity, swipe down will have a positive velocity. Negate this
            // values so that the logic is more intuitive.
            int targetState = getTargetSheetState(
                    getSheetOffsetFromBottom() + getFlingDistance(-velocityY), -velocityY);
            setSheetState(targetState, true);
            mIsScrolling = false;

            return true;
        }
    }

    /**
     * Constructor for inflation from XML.
     * @param context An Android context.
     * @param atts The XML attributes.
     */
    public BottomSheet(Context context, AttributeSet atts) {
        super(context, atts);

        setOrientation(LinearLayout.VERTICAL);

        mVelocityTracker = VelocityTracker.obtain();

        mGestureDetector = new GestureDetector(context, new BottomSheetSwipeDetector());
        mGestureDetector.setIsLongpressEnabled(false);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        // The incoming motion event may have been adjusted by the view sending it down. Create a
        // motion event with the raw (x, y) coordinates of the original so the gesture detector
        // functions properly.
        mGestureDetector.onTouchEvent(createRawMotionEvent(e));
        return mIsScrolling;
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        // The down event is interpreted above in onInterceptTouchEvent, it does not need to be
        // interpreted a second time.
        if (e.getActionMasked() != MotionEvent.ACTION_DOWN) {
            mGestureDetector.onTouchEvent(createRawMotionEvent(e));
        }

        // If the user is scrolling and the event is a cancel or up action, update scroll state
        // and return.
        if (e.getActionMasked() == MotionEvent.ACTION_UP
                || e.getActionMasked() == MotionEvent.ACTION_CANCEL) {
            mIsScrolling = false;

            mVelocityTracker.computeCurrentVelocity(1000);

            // If an animation was not created to settle the sheet at some state, do it now.
            if (mSettleAnimator == null) {
                // Negate velocity so a positive number indicates a swipe up.
                float currentVelocity = -mVelocityTracker.getYVelocity();
                int targetState = getTargetSheetState(getSheetOffsetFromBottom(), currentVelocity);

                setSheetState(targetState, true);
            }
        }

        return true;
    }

    @Override
    public boolean gatherTransparentRegion(Region region) {
        // TODO(mdjones): Figure out what this should actually be set to since the view animates
        // without necessarily calling this method again.
        region.setEmpty();
        return true;
    }

    /**
     * Adds layout change listeners to the views that the bottom sheet depends on. Namely the
     * heights of the root view and control container are important as they are used in many of the
     * calculations in this class.
     * @param root The container of the bottom sheet.
     * @param controlContainer The container for the toolbar.
     */
    public void init(View root, View controlContainer) {
        mToolbarHeight = controlContainer.getHeight();
        mCurrentState = SHEET_STATE_PEEK;

        // Listen to height changes on the root.
        root.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                mContainerHeight = bottom - top;
                updateSheetPeekHeight();
                setSheetState(mCurrentState, false);
            }
        });

        // Listen to height changes on the toolbar.
        controlContainer.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                mToolbarHeight = bottom - top;
                updateSheetPeekHeight();
                setSheetState(mCurrentState, false);
            }
        });
    }

    /**
     * Creates an unadjusted version of a MotionEvent.
     * @param e The original event.
     * @return The unadjusted version of the event.
     */
    private MotionEvent createRawMotionEvent(MotionEvent e) {
        MotionEvent rawEvent = MotionEvent.obtain(e);
        rawEvent.setLocation(e.getRawX(), e.getRawY());
        return rawEvent;
    }

    /**
     * Updates the bottom sheet's peeking height.
     */
    private void updateSheetPeekHeight() {
        if (mContainerHeight <= 0) return;

        mStateRatios[0] = mToolbarHeight / mContainerHeight;
    }

    /**
     * Cancels and nulls the height animation if it exists.
     */
    private void cancelAnimation() {
        if (mSettleAnimator == null) return;
        mSettleAnimator.cancel();
        mSettleAnimator = null;
    }

    /**
     * Creates the sheet's animation to a target state.
     * @param targetState The target state.
     */
    private void createSettleAnimation(@SheetState int targetState) {
        mCurrentState = targetState;
        mSettleAnimator = ObjectAnimator.ofFloat(
                this, View.TRANSLATION_Y, mContainerHeight - getSheetHeightForState(targetState));
        mSettleAnimator.setDuration(BASE_ANIMATION_DURATION_MS);
        mSettleAnimator.setInterpolator(mInterpolator);

        // When the animation is canceled or ends, reset the handle to null.
        mSettleAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animator) {
                mSettleAnimator = null;
            }
        });

        mSettleAnimator.start();
    }

    /**
     * Gets the distance of a fling based on the velocity and the base animation time. This formula
     * assumes the deceleration curve is quadratic (t^2), hence the displacement formula should be:
     * displacement = initialVelocity * duration / 2.
     * @param velocity The velocity of the fling.
     * @return The distance the fling would cover.
     */
    private float getFlingDistance(float velocity) {
        // This includes conversion from seconds to ms.
        return velocity * BASE_ANIMATION_DURATION_MS / 2000f;
    }

    /**
     * Gets the maximum offset of the bottom sheet.
     * @return The max offset.
     */
    private float getMaxOffset() {
        return mStateRatios[mStateRatios.length - 1] * mContainerHeight;
    }

    /**
     * Gets the minimum offset of the bottom sheet.
     * @return The min offset.
     */
    private float getMinOffset() {
        return mStateRatios[0] * mContainerHeight;
    }

    /**
     * Gets the sheet's offset from the bottom of the screen.
     * @return The sheet's distance from the bottom of the screen.
     */
    private float getSheetOffsetFromBottom() {
        return mContainerHeight - getTranslationY();
    }

    /**
     * Sets the sheet's offset relative to the bottom of the screen.
     * @param offset The offset that the sheet should be.
     */
    private void setSheetOffsetFromBottom(float offset) {
        setTranslationY(mContainerHeight - offset);
    }

    /**
     * Moves the sheet to the provided state.
     * @param state The state to move the panel to.
     * @param animate If true, the sheet will animate to the provided state, otherwise it will
     *                move there instantly.
     */
    private void setSheetState(@SheetState int state, boolean animate) {
        mCurrentState = state;

        if (animate) {
            createSettleAnimation(state);
        } else {
            setSheetOffsetFromBottom(getSheetHeightForState(state));
        }
    }

    /**
     * Gets the height of the bottom sheet based on a provided state.
     * @param state The state to get the height from.
     * @return The height of the sheet at the provided state.
     */
    private float getSheetHeightForState(@SheetState int state) {
        return mStateRatios[state] * mContainerHeight;
    }

    /**
     * Gets the target state of the sheet based on the sheet's height and velocity.
     * @param sheetHeight The current height of the sheet.
     * @param yVelocity The current Y velocity of the sheet. This is only used for determining the
     *                  scroll or fling direction. If this value is positive, the movement is from
     *                  bottom to top.
     * @return The target state of the bottom sheet.
     */
    private int getTargetSheetState(float sheetHeight, float yVelocity) {
        if (sheetHeight <= getMinOffset()) return SHEET_STATE_PEEK;
        if (sheetHeight >= getMaxOffset()) return SHEET_STATE_FULL;

        // First, find the two states that the sheet height is between.
        int nextState = sStates[0];
        int prevState = nextState;
        for (int i = 0; i < sStates.length; i++) {
            prevState = nextState;
            nextState = sStates[i];
            // The values in PanelState are ascending, they should be kept that way in order for
            // this to work.
            if (sheetHeight >= getSheetHeightForState(prevState)
                    && sheetHeight < getSheetHeightForState(nextState)) {
                break;
            }
        }

        // If the desired height is close enough to a certain state, depending on the direction of
        // the velocity, move to that state.
        float lowerBound = getSheetHeightForState(prevState);
        float distance = getSheetHeightForState(nextState) - lowerBound;

        float inverseThreshold = 1.0f - THRESHOLD_TO_NEXT_STATE;
        float thresholdToNextState = yVelocity < 0.0f ? THRESHOLD_TO_NEXT_STATE : inverseThreshold;

        if ((sheetHeight - lowerBound) / distance > thresholdToNextState) {
            return nextState;
        }
        return prevState;
    }
}
