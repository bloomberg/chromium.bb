// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone.stack;

import android.content.Context;

import org.chromium.chrome.browser.compositor.layouts.phone.StackLayoutBase;

/**
 * The non-overlapping tab stack we use when the HorizontalTabSwitcherAndroid flag is enabled.
 */
public class NonOverlappingStack extends Stack {
    /**
     * The percentage of the screen that defines the spacing between tabs by default (no pinch).
     */
    private static final float SPACING_SCREEN = 1.0f;

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
     * @param layout The parent layout.
     */
    public NonOverlappingStack(Context context, StackLayoutBase layout) {
        super(context, layout);
    }

    @Override
    protected boolean evenOutTabs(float amount, boolean allowReverseDirection) {
        // Nothing to do here; tabs are always a fixed distance apart in NonOverlappingStack (except
        // during tab close/un-close animations)
        return false;
    }

    @Override
    public void onPinch(long time, float x0, float y0, float x1, float y1, boolean firstEvent) {
        return;
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
    protected boolean shouldCloseGapsBetweenTabs() {
        return false;
    }

    @Override
    protected int computeSpacing(int layoutTabCount) {
        return (int) Math.round(getScrollDimensionSize() * StackAnimation.SCALE_AMOUNT
                + EXTRA_SPACE_BETWEEN_TABS_DP);
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
}
