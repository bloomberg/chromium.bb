// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.PropertyValuesHolder;
import android.content.Context;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.GestureDetector.SimpleOnGestureListener;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.AccelerateInterpolator;
import android.widget.FrameLayout;

import org.chromium.content.browser.ContentView;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.ui.UiUtils;

/**
 * View that appears on the screen as the user scrolls on the page and can be swiped away.
 * Meant to be tacked onto the {@link org.chromium.content.browser.ContentView} layer and alerted
 * when either the page scroll position or viewport size changes.
 *
 * GENERAL BEHAVIOR
 * This View is brought onto the screen by sliding upwards from the bottom of the screen.  Afterward
 * the View slides onto and off of the screen vertically as the user scrolls upwards or
 * downwards on the page.  Users dismiss the View by swiping it away horizontally.
 *
 * VERTICAL SCROLL CALCULATIONS
 * To determine how close the user is to the top of the page, the View must not only be informed of
 * page scroll position changes, but also of changes in the viewport size (which happens as the
 * omnibox appears and disappears, or as the page rotates e.g.).  When the viewport size gradually
 * shrinks, the user is most likely to be scrolling the page downwards while the omnibox comes back
 * into view.
 *
 * When the user first begins scrolling the page, both the scroll position and the viewport size are
 * summed and recorded together.  This is because a pixel change in the viewport height is
 * equivalent to a pixel change in the content's scroll offset:
 * - As the user scrolls the page downward, either the viewport height will increase (as the omnibox
 *   is slid off of the screen) or the content scroll offset will increase.
 * - As the user scrolls the page upward, either the viewport height will decrease (as the omnibox
 *   is brought back onto the screen) or the content scroll offset will decrease.
 *
 * As the scroll offset or the viewport height are updated via a scroll or fling, the difference
 * from the initial value is used to determine the View's Y-translation.  If a gesture is stopped,
 * the View will be snapped back into the center of the screen or entirely off of the screen, based
 * on how much of the banner is visible, or where the user is currently located on the page.
 *
 * HORIZONTAL SCROLL CALCULATIONS
 * Horizontal drags and swipes are used to dismiss the View.  Translating the View far enough
 * horizontally (with "enough" defined by the DISMISS_SWIPE_THRESHOLD AND DISMISS_FLING_THRESHOLD)
 * triggers an animation that removes the View from the hierarchy.  Failing to meet the threshold
 * will result in the View being translated back to the center of the screen.
 *
 * Because the fling velocity handed in by Android is highly inaccurate and often indicates
 * that a fling is moving in an opposite direction than expected, the scroll direction is tracked
 * to determine which direction the user was dragging the View when the fling was initiated.  When a
 * fling is completed, the more forgiving FLING_THRESHOLD is used to determine how far a user must
 * swipe to dismiss the View rather than try to use the fling velocity.
 */
public abstract class SwipableOverlayView extends FrameLayout {
    private static final float ALPHA_THRESHOLD = 0.25f;
    private static final float DISMISS_FLING_THRESHOLD = 0.1f;
    private static final float DISMISS_SWIPE_THRESHOLD = 0.75f;
    private static final float FULL_THRESHOLD = 0.5f;
    private static final float VERTICAL_FLING_SHOW_THRESHOLD = 0.2f;
    private static final float VERTICAL_FLING_HIDE_THRESHOLD = 0.9f;
    protected static final float ZERO_THRESHOLD = 0.001f;

    private static final int GESTURE_NONE = 0;
    private static final int GESTURE_SCROLLING = 1;
    private static final int GESTURE_FLINGING = 2;

    private static final int DRAGGED_LEFT = -1;
    private static final int DRAGGED_CANCEL = 0;
    private static final int DRAGGED_RIGHT = 1;

    protected static final long MS_ANIMATION_DURATION = 300;

    // Detects when the user is dragging the View.
    private final GestureDetector mGestureDetector;

    // Detects when the user is dragging the ContentView.
    private final GestureStateListener mGestureStateListener;

    // Tracks whether the user is scrolling or flinging.
    private int mGestureState;

    // Animation currently being used to translate the View.
    private AnimatorSet mCurrentAnimation;

    // Whether or not the current animation is dismissing the View.
    private boolean mIsAnimationRemovingView;

    // Direction the user is horizontally dragging.
    private int mDragDirection;

    // Used to determine when the layout has changed and the Viewport must be updated.
    private int mParentHeight;

    // Location of the View when the current gesture was first started.
    private float mInitialTranslationY;

    // Offset from the top of the page when the current gesture was first started.
    private int mInitialOffsetY;

    // How tall the View is, including its margins.
    private int mTotalHeight;

    /**
     * Creates a SwipableOverlayView.
     * @param context Context for acquiring resources.
     * @param attrs Attributes from the XML layout inflation.
     */
    public SwipableOverlayView(Context context, AttributeSet attrs) {
        super(context, attrs);
        SimpleOnGestureListener gestureListener = createGestureListener();
        mGestureDetector = new GestureDetector(context, gestureListener);
        mGestureStateListener = createGestureStateListener();
        mGestureState = GESTURE_NONE;
    }

    /**
     * Adds this View to the given ContentView.
     * @param layout Layout to add this View to.
     */
    protected void addToView(ContentView contentView) {
        contentView.addView(this, 0, createLayoutParams());
        contentView.getContentViewCore().addGestureStateListener(mGestureStateListener);

        // Listen for the layout to know when to animate the banner coming onto the screen.
        addOnLayoutChangeListener(createLayoutChangeListener());
    }

    /**
     * Creates a set of LayoutParams that makes the View hug the bottom of the screen.  Override it
     * for other types of behavior.
     * @return LayoutParams for use when adding the View to its parent.
     */
    protected ViewGroup.MarginLayoutParams createLayoutParams() {
        return new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT,
                Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
    }

    /**
     * Removes the View from its parent.
     */
    boolean removeFromParent() {
        if (getParent() instanceof ContentView) {
            ContentView parent = (ContentView) getParent();
            parent.removeView(this);
            parent.getContentViewCore().removeGestureStateListener(mGestureStateListener);
            return true;
        }
        return false;
    }

    /**
     * See {@link #android.view.ViewGroup.onLayout(boolean, int, int, int, int)}.
     */
    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        // Hide the banner when the keyboard is showing.
        boolean keyboardIsShowing = UiUtils.isKeyboardShowing(getContext(), this);
        setVisibility(keyboardIsShowing ? INVISIBLE : VISIBLE);

        // Update the viewport height when the parent View's height changes (e.g. after rotation).
        int currentParentHeight = getParent() == null ? 0 : ((View) getParent()).getHeight();
        if (mParentHeight != currentParentHeight) {
            mParentHeight = currentParentHeight;
            mGestureState = GESTURE_NONE;
            if (mCurrentAnimation != null) mCurrentAnimation.end();
        }

        // Update the known effective height of the View.
        mTotalHeight = getMeasuredHeight();
        if (getLayoutParams() instanceof MarginLayoutParams) {
            MarginLayoutParams params = (MarginLayoutParams) getLayoutParams();
            mTotalHeight += params.topMargin + params.bottomMargin;
        }

        super.onLayout(changed, l, t, r, b);
    }

    /**
     * See {@link #android.view.View.onTouchEvent(MotionEvent)}.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (mGestureDetector.onTouchEvent(event)) return true;
        if (mCurrentAnimation != null) return true;

        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            createHorizontalAnimation();
            return true;
        }
        return false;
    }

    /**
     * Creates a listener that monitors horizontal dismissal gestures performed on the View.
     * @return The SimpleOnGestureListener that will monitor the View.
     */
    private SimpleOnGestureListener createGestureListener() {
        return new SimpleOnGestureListener() {
            @Override
            public boolean onDown(MotionEvent e) {
                mDragDirection = DRAGGED_CANCEL;
                mGestureState = GESTURE_NONE;
                return true;
            }

            @Override
            public boolean onScroll(MotionEvent e1, MotionEvent e2, float distX, float distY) {
                mGestureState = GESTURE_SCROLLING;
                float distance = e2.getX() - e1.getX();
                setTranslationX(getTranslationX() + distance);
                setAlpha(calculateAnimationAlpha());

                // Because the fling velocity is unreliable, we track what direction the user is
                // dragging the View from here.
                mDragDirection = distance < 0 ? DRAGGED_LEFT : DRAGGED_RIGHT;
                return true;
            }

            @Override
            public boolean onFling(MotionEvent e1, MotionEvent e2, float vX, float vY) {
                mGestureState = GESTURE_FLINGING;
                createHorizontalAnimation();
                return true;
            }

            @Override
            public boolean onSingleTapConfirmed(MotionEvent e) {
                onViewClicked();
                return true;
            }
        };
    }

    /**
     * Creates a listener than monitors the ContentViewCore for scrolls and flings.
     * The listener updates the location of this View to account for the user's gestures.
     * @return GestureStateListener to send to the ContentViewCore.
     */
    private GestureStateListener createGestureStateListener() {
        return new GestureStateListener() {
            @Override
            public void onFlingStartGesture(int vx, int vy, int scrollOffsetY, int scrollExtentY) {
                if (!cancelCurrentAnimation()) return;
                if (mGestureState == GESTURE_NONE) beginGesture(scrollOffsetY, scrollExtentY);
                mGestureState = GESTURE_FLINGING;
            }

            @Override
            public void onFlingEndGesture(int scrollOffsetY, int scrollExtentY) {
                if (mGestureState != GESTURE_FLINGING) return;
                mGestureState = GESTURE_NONE;

                int finalOffsetY = computeScrollDifference(scrollOffsetY, scrollExtentY);
                boolean isScrollingDownward = finalOffsetY > 0;

                boolean isVisibleInitially = mInitialTranslationY < mTotalHeight;
                float percentageVisible = 1.0f - (getTranslationY() / mTotalHeight);
                float visibilityThreshold = isVisibleInitially
                        ? VERTICAL_FLING_HIDE_THRESHOLD : VERTICAL_FLING_SHOW_THRESHOLD;
                boolean isVisibleEnough = percentageVisible > visibilityThreshold;

                boolean show = !isScrollingDownward;
                if (isVisibleInitially) {
                    // Check if the banner was moving off-screen.
                    boolean isHiding = getTranslationY() > mInitialTranslationY;
                    show &= isVisibleEnough || !isHiding;
                } else {
                    // When near the top of the page, there's not much room left to scroll.
                    boolean isNearTopOfPage = finalOffsetY < (mTotalHeight * FULL_THRESHOLD);
                    show &= isVisibleEnough || isNearTopOfPage;
                }
                createVerticalSnapAnimation(show);
            }

            @Override
            public void onScrollStarted(int scrollOffsetY, int scrollExtentY) {
                if (!cancelCurrentAnimation()) return;
                if (mGestureState == GESTURE_NONE) beginGesture(scrollOffsetY, scrollExtentY);
                mGestureState = GESTURE_SCROLLING;
            }

            @Override
            public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
                if (mGestureState != GESTURE_SCROLLING) return;
                mGestureState = GESTURE_NONE;

                int finalOffsetY = computeScrollDifference(scrollOffsetY, scrollExtentY);
                boolean isNearTopOfPage = finalOffsetY < (mTotalHeight * FULL_THRESHOLD);
                boolean isVisibleEnough = getTranslationY() < mTotalHeight * FULL_THRESHOLD;
                createVerticalSnapAnimation(isNearTopOfPage || isVisibleEnough);
            }

            @Override
            public void onScrollOffsetOrExtentChanged(int scrollOffsetY, int scrollExtentY) {
                // This function is called for both fling and scrolls.
                if (mGestureState == GESTURE_NONE || !cancelCurrentAnimation()) return;

                float translation = mInitialTranslationY
                        + computeScrollDifference(scrollOffsetY, scrollExtentY);
                translation = Math.max(0.0f, Math.min(mTotalHeight, translation));
                setTranslationY(translation);
            }
        };
    }

    /**
     * Creates a listener that is used only to animate the View coming onto the screen.
     * @return The SimpleOnGestureListener that will monitor the View.
     */
    private View.OnLayoutChangeListener createLayoutChangeListener() {
        return new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                removeOnLayoutChangeListener(this);

                // Animate the banner coming in from the bottom of the screen.
                setTranslationY(mTotalHeight);
                createVerticalSnapAnimation(true);
                mCurrentAnimation.start();
            }
        };
    }

    /**
     * Create an animation that snaps the View into position vertically.
     * @param visible If true, snaps the View to the bottom-center of the screen.  If false,
     *                translates the View below the bottom-center of the screen so that it is
     *                effectively invisible.
     */
    void createVerticalSnapAnimation(boolean visible) {
        float translationY = visible ? 0.0f : mTotalHeight;
        float yDifference = Math.abs(translationY - getTranslationY()) / mTotalHeight;
        long duration = (long) (MS_ANIMATION_DURATION * yDifference);
        createAnimation(1.0f, 0, translationY, duration, false);
    }

    /**
     * Dismisses the banner, animating the banner moving vertically off of the screen if needed.
     */
    void dismiss() {
        if (getParent() == null) return;

        float translationY = mTotalHeight;
        float yDifference = Math.abs(translationY - getTranslationY()) / mTotalHeight;
        long duration = (long) (MS_ANIMATION_DURATION * yDifference);
        createAnimation(1.0f, 0, translationY, duration, true);
    }

    /**
     * Calculates how transparent the View should be.
     *
     * The transparency value is proportional to how far the View has been swiped away from the
     * center of the screen.  The {@link ALPHA_THRESHOLD} determines at what point the banner should
     * start fading away.
     * @return The alpha value to use for the View.
     */
    private float calculateAnimationAlpha() {
        float percentageSwiped = Math.abs(getTranslationX() / getWidth());
        float percentageAdjusted = Math.max(0.0f, percentageSwiped - ALPHA_THRESHOLD);
        float alphaRange = 1.0f - ALPHA_THRESHOLD;
        return 1.0f - percentageAdjusted / alphaRange;
    }

    private int computeScrollDifference(int scrollOffsetY, int scrollExtentY) {
        return scrollOffsetY + scrollExtentY - mInitialOffsetY;
    }

    /**
     * Horizontally slide the View either to the center of the screen or off of it for dismissal.
     * If the animation translates the View off the screen, the View is removed from the hierarchy
     * and marked as a manual removal.
     */
    private void createHorizontalAnimation() {
        // Determine where the banner needs to move. Because of the unreliability of the fling
        // velocity, we ignore it and instead rely on the direction the user was last dragging the
        // View.  Moreover, we lower the translation threshold for dismissal.
        boolean isFlinging = mGestureState == GESTURE_FLINGING;
        float dismissThreshold =
                getWidth() * (isFlinging ? DISMISS_FLING_THRESHOLD : DISMISS_SWIPE_THRESHOLD);
        boolean isSwipedFarEnough = Math.abs(getTranslationX()) > dismissThreshold;
        boolean isFlungInSameDirection =
                (getTranslationX() < 0.0f) == (mDragDirection == DRAGGED_LEFT);
        if (!isSwipedFarEnough || (isFlinging && !isFlungInSameDirection)) {
            mDragDirection = DRAGGED_CANCEL;
        }

        // Set up the animation parameters.
        float finalAlpha = mDragDirection == DRAGGED_CANCEL ? 1.0f : 0.0f;
        float finalX = mDragDirection * getWidth();
        float xDifference = Math.abs(finalX - getTranslationX()) / getWidth();
        long duration = (long) (MS_ANIMATION_DURATION * xDifference);

        // Dismiss the banner if it's going off the screen.
        boolean translatedOffScreen = mDragDirection != DRAGGED_CANCEL;

        createAnimation(finalAlpha, finalX, getTranslationY(), duration, translatedOffScreen);
    }

    /**
     * Creates an animation that slides the View to the given location and visibility.
     * @param alpha How opaque the View should be at the end.
     * @param x X-coordinate of the final translation.
     * @param y Y-coordinate of the final translation.
     * @param duration How long the animation should run for.
     * @param remove If true, remove the View from its parent ViewGroup.
     */
    private void createAnimation(float alpha, float x, float y, long duration,
            final boolean remove) {
        Animator alphaAnimator =
                ObjectAnimator.ofPropertyValuesHolder(this,
                        PropertyValuesHolder.ofFloat("alpha", getAlpha(), alpha));
        Animator translationXAnimator =
                ObjectAnimator.ofPropertyValuesHolder(this,
                        PropertyValuesHolder.ofFloat("translationX", getTranslationX(), x));
        Animator translationYAnimator =
                ObjectAnimator.ofPropertyValuesHolder(this,
                        PropertyValuesHolder.ofFloat("translationY", getTranslationY(), y));

        mIsAnimationRemovingView = remove;
        mCurrentAnimation = new AnimatorSet();
        mCurrentAnimation.setDuration(duration);
        mCurrentAnimation.playTogether(alphaAnimator, translationXAnimator, translationYAnimator);
        mCurrentAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mGestureState = GESTURE_NONE;
                mCurrentAnimation = null;
                mIsAnimationRemovingView = false;
                if (remove) removeFromParent();
            }
        });
        mCurrentAnimation.setInterpolator(new AccelerateInterpolator());
        mCurrentAnimation.start();
    }

    /**
     * Records the conditions of the page when a gesture is initiated.
     */
    private void beginGesture(int scrollOffsetY, int scrollExtentY) {
        mInitialTranslationY = getTranslationY();
        boolean isInitiallyVisible = mInitialTranslationY < mTotalHeight;
        int startingY = isInitiallyVisible ? scrollOffsetY : Math.min(scrollOffsetY, mTotalHeight);
        mInitialOffsetY = startingY + scrollExtentY;
    }

    /**
     * Cancels the current animation, if the View isn't being dismissed.
     * @return True if the animation was canceled or wasn't running, false otherwise.
     */
    private boolean cancelCurrentAnimation() {
        if (mIsAnimationRemovingView) return false;
        if (mCurrentAnimation != null) mCurrentAnimation.cancel();
        return true;
    }

    /**
     * Called when the View has been clicked.
     */
    protected abstract void onViewClicked();
}
