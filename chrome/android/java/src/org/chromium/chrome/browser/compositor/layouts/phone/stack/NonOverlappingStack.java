// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone.stack;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.content.Context;
import android.support.annotation.IntDef;

import org.chromium.chrome.browser.compositor.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimator;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimator.AnimatorUpdateListener;
import org.chromium.chrome.browser.compositor.layouts.phone.StackLayoutBase;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collection;

/**
 * The non-overlapping tab stack we use when the HorizontalTabSwitcherAndroid flag is enabled.
 */
public class NonOverlappingStack extends Stack {
    @IntDef({SWITCH_DIRECTION_LEFT, SWITCH_DIRECTION_RIGHT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SwitchDirection {}
    public static final int SWITCH_DIRECTION_LEFT = 0;
    public static final int SWITCH_DIRECTION_RIGHT = 1;

    /**
     * The scale the tabs should be shown at when there's exactly one tab open.
     */
    private static final float SCALE_FRACTION_SINGLE_TAB = 0.80f;

    /**
     * The scale the tabs should be shown at when there are two or more tabs open.
     */
    private static final float SCALE_FRACTION_MULTIPLE_TABS = 0.60f;

    /**
     * The percentage of the screen that defines the spacing between tabs by default (no pinch).
     */
    private static final float SPACING_SCREEN = 1.0f;

    /**
     * Controls how far we slide over the (up to) three visible tabs for the switch away and switch
     * to animations (multiple of mSpacing).
     */
    private static final float SWITCH_ANIMATION_SPACING_MULTIPLE = 2.5f;

    /**
     * Duration of the switch away animation (in milliseconds).
     */
    private static final int SWITCH_AWAY_ANIMATION_DURATION = 250;

    /**
     * Duration of the switch to animation (in milliseconds).
     */
    private static final int SWITCH_TO_ANIMATION_DURATION = 250;

    /**
     * Adjustment to add a fixed amount of space between the tabs that's not based on a percentage
     * of the screen (if were 0, the tab borders would actually overlap in the current
     * implementation).
     */
    private static final float EXTRA_SPACE_BETWEEN_TABS_DP = 25.0f;

    /**
     * How much the stack should adjust the y position of each LayoutTab in portrait mode (as a
     * fraction of the amount space that would be above and below the tab if it were centered).
     */
    private static final float STACK_PORTRAIT_Y_OFFSET_PROPORTION = 0.f;

    /**
     * How much the stack should adjust the x position of each LayoutTab in landscape mode (as a
     * fraction of the amount space that would be to the left and right of the tab if it were
     * centered).
     */
    private static final float STACK_LANDSCAPE_START_OFFSET_PROPORTION = 0.f;

    /**
     * How much the stack should adjust the x position of each LayoutTab in portrait mode (as a
     * fraction of the amount space that would be above and below the tab if it were centered).
     */
    private static final float STACK_LANDSCAPE_Y_OFFSET_PROPORTION = 0.f;

    /**
     * Multiplier for adjusting the scrolling friction from the amount provided by
     * ViewConfiguration.
     */
    private static final float FRICTION_MULTIPLIER = 0.2f;

    /**
     * Used to prevent mScrollOffset from being changed as a result of clamping during the switch
     * away/switch to animations.
     */
    private boolean mSuppressScrollClamping;

    /**
     * Whether or not the current stack has been "switched away" by having runSwitchAwayAnimation()
     * called. Calling runSwitchToAnimation() resets this back to false. Checking this variable lets
     * us avoid re-playing animations if they're triggered multiple times.
     */
    private boolean mSwitchedAway;

    /**
     * @param layout The parent layout.
     */
    public NonOverlappingStack(Context context, StackLayoutBase layout) {
        super(context, layout);
    }

    private int getNonDyingTabCount() {
        if (mStackTabs == null) return 0;

        int dyingCount = 0;
        for (int i = 0; i < mStackTabs.length; i++) {
            if (mStackTabs[i].isDying()) dyingCount++;
        }
        return mStackTabs.length - dyingCount;
    }

    @Override
    public float getScaleAmount() {
        if (getNonDyingTabCount() > 1) return SCALE_FRACTION_MULTIPLE_TABS;
        return SCALE_FRACTION_SINGLE_TAB;
    }

    @Override
    protected boolean evenOutTabs(float amount, boolean allowReverseDirection) {
        // Nothing to do here; tabs are always a fixed distance apart in NonOverlappingStack (except
        // during tab close/un-close animations)
        return false;
    }

    private void updateScrollSnap() {
        mScroller.setFrictionMultiplier(FRICTION_MULTIPLIER);
        // This is what computeSpacing() returns when there are >= 2 tabs
        final int snapDistance =
                (int) Math.round(getScrollDimensionSize() * SCALE_FRACTION_MULTIPLE_TABS
                        + EXTRA_SPACE_BETWEEN_TABS_DP);
        // Really we're scrolling in the x direction, but the scroller is always wired up to the y
        // direction for both portrait and landscape mode.
        mScroller.setYSnapDistance(snapDistance);
    }

    @Override
    public void contextChanged(Context context) {
        super.contextChanged(context);
        updateScrollSnap();
    }

    @Override
    public void notifySizeChanged(float width, float height, int orientation) {
        super.notifySizeChanged(width, height, orientation);
        updateScrollSnap();
    }

    @Override
    public void onPinch(long time, float x0, float y0, float x1, float y1, boolean firstEvent) {
        return;
    }

    @Override
    protected void springBack(long time) {
        if (mScroller.isFinished()) {
            int newTarget = Math.round(mScrollTarget / mSpacing) * mSpacing;
            mScroller.springBack(0, (int) mScrollTarget, 0, 0, newTarget, newTarget, time);
            setScrollTarget(newTarget, false);
            requestUpdate();
        }
    }

    @Override
    protected float getSpacingScreen() {
        return SPACING_SCREEN;
    }

    @Override
    protected boolean shouldStackTabsAtTop() {
        return false;
    }

    @Override
    protected boolean shouldStackTabsAtBottom() {
        return false;
    }

    @Override
    protected float getStackPortraitYOffsetProportion() {
        return STACK_PORTRAIT_Y_OFFSET_PROPORTION;
    }

    @Override
    protected float getStackLandscapeStartOffsetProportion() {
        return STACK_LANDSCAPE_START_OFFSET_PROPORTION;
    }

    @Override
    protected float getStackLandscapeYOffsetProportion() {
        return STACK_LANDSCAPE_Y_OFFSET_PROPORTION;
    }

    @Override
    protected int computeReferenceIndex() {
        return -Math.round(mScrollTarget / mSpacing);
    }

    @Override
    protected boolean shouldCloseGapsBetweenTabs() {
        return false;
    }

    @Override
    protected float getMinScroll(boolean allowUnderScroll) {
        if (mSuppressScrollClamping) return -Float.MAX_VALUE;
        return super.getMinScroll(allowUnderScroll);
    }

    @Override
    protected int computeSpacing(int layoutTabCount) {
        return (int) Math.round(
                getScrollDimensionSize() * getScaleAmount() + EXTRA_SPACE_BETWEEN_TABS_DP);
    }

    @Override
    protected void resetAllScrollOffset() {
        if (mTabList == null) return;

        // We want to focus on the tab before the currently-selected one, since we assume that's
        // the one the user is most likely to switch to.
        int index = mTabList.index();
        if (index != 0) index -= 1;

        // Reset the tabs' scroll offsets.
        if (mStackTabs != null) {
            for (int i = 0; i < mStackTabs.length; i++) {
                mStackTabs[i].setScrollOffset(screenToScroll(i * mSpacing));
            }
        }

        // Reset the overall scroll offset.
        mScrollOffset = -index * mSpacing;
        setScrollTarget(mScrollOffset, false);
    }

    // NonOverlappingStack uses linear scrolling, so screenToScroll() and scrollToScreen() are both
    // just the identity function.
    @Override
    public float screenToScroll(float screenSpace) {
        return screenSpace;
    }

    @Override
    public float scrollToScreen(float scrollSpace) {
        return scrollSpace;
    }

    @Override
    public float getMaxTabHeight() {
        // We want to maintain a constant tab height (via cropping) even as the width is changed as
        // a result of changing the scale.
        if (getNonDyingTabCount() > 1) return super.getMaxTabHeight();
        return (SCALE_FRACTION_MULTIPLE_TABS / SCALE_FRACTION_SINGLE_TAB) * super.getMaxTabHeight();
    }

    /**
     * Animates the (up to 3) visible tabs sliding off screen.
     * @param direction Whether the tabs should slide off the left or right side of the screen.
     */
    public void runSwitchAwayAnimation(@SwitchDirection int direction) {
        if (mStackTabs == null || mSwitchedAway) {
            mSwitchedAway = true;
            mLayout.onSwitchAwayFinished();
            return;
        }

        mSwitchedAway = true;

        mSuppressScrollClamping = true;

        CompositorAnimationHandler handler = mLayout.getAnimationHandler();
        Collection<Animator> animationList = new ArrayList<>();

        int centeredTab = Math.round(-mScrollOffset / mSpacing);
        for (int i = centeredTab - 1; i <= centeredTab + 1; i++) {
            if (i < 0 || i >= mStackTabs.length) continue;
            StackTab tab = mStackTabs[i];

            float endOffset;
            if (direction == SWITCH_DIRECTION_LEFT) {
                endOffset = -SWITCH_ANIMATION_SPACING_MULTIPLE * mSpacing + tab.getScrollOffset();
            } else {
                endOffset = SWITCH_ANIMATION_SPACING_MULTIPLE * mSpacing + tab.getScrollOffset();
            }

            CompositorAnimator animation =
                    CompositorAnimator.ofFloatProperty(handler, tab, StackTab.SCROLL_OFFSET,
                            tab.getScrollOffset(), endOffset, SWITCH_AWAY_ANIMATION_DURATION);
            // TODO(https://crbug.com/848475) Fix StackLayoutBase and Stack so the animation works
            // properly without this listener.
            animation.addUpdateListener(new AnimatorUpdateListener() {
                @Override
                public void onAnimationUpdate(CompositorAnimator animation) {
                    requestUpdate();
                }
            });
            animationList.add(animation);
        }

        AnimatorSet set = new AnimatorSet();
        set.playTogether(animationList);
        set.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator a) {
                // If the user pressed the incognito button with one finger while dragging the stack
                // with another, we might not be centered on a tab. We therefore need to enforce
                // this after the animation finishes to avoid odd behavior if/when the user returns
                // to this stack.

                mScrollOffset = Math.round(mScrollOffset / mSpacing) * mSpacing;
                forceScrollStop();
                mLayout.onSwitchAwayFinished();
            }
        });
        set.start();
    }

    /**
     * Animates the (up to 3) tabs were slid off the screen by runSwitchAwayAnimation() back onto
     * the screen.
     * @param direction Whether the tabs should slide in from the left or right side of the screen.
     */
    public void runSwitchToAnimation(@SwitchDirection int direction) {
        if (mStackTabs == null || !mSwitchedAway) {
            mSwitchedAway = false;
            mLayout.onSwitchToFinished();
            return;
        }

        mSwitchedAway = false;

        CompositorAnimationHandler handler = mLayout.getAnimationHandler();
        Collection<Animator> animationList = new ArrayList<>();

        int centeredTab = Math.round(-mScrollOffset / mSpacing);
        for (int i = centeredTab - 1; i <= centeredTab + 1; i++) {
            if (i < 0 || i >= mStackTabs.length) continue;
            StackTab tab = mStackTabs[i];

            float startOffset;
            if (direction == SWITCH_DIRECTION_LEFT) {
                startOffset = SWITCH_ANIMATION_SPACING_MULTIPLE * mSpacing + tab.getScrollOffset();
            } else {
                startOffset = -SWITCH_ANIMATION_SPACING_MULTIPLE * mSpacing + tab.getScrollOffset();
            }

            CompositorAnimator animation =
                    CompositorAnimator.ofFloatProperty(handler, tab, StackTab.SCROLL_OFFSET,
                            startOffset, i * mSpacing, SWITCH_TO_ANIMATION_DURATION);
            animation.addUpdateListener(new AnimatorUpdateListener() {
                @Override
                public void onAnimationUpdate(CompositorAnimator animation) {
                    requestUpdate();
                }
            });
            animationList.add(animation);
        }

        AnimatorSet set = new AnimatorSet();
        set.playTogether(animationList);
        set.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator a) {
                mSuppressScrollClamping = false;
                mLayout.onSwitchToFinished();
            }
        });
        set.start();
    }
}
