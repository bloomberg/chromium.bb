// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone.stack;

import static org.chromium.chrome.browser.compositor.layouts.ChromeAnimation.AnimatableAnimation.addAnimation;

import org.chromium.chrome.browser.compositor.layouts.ChromeAnimation;
import org.chromium.chrome.browser.compositor.layouts.ChromeAnimation.Animatable;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;

abstract class StackAnimationLandscape extends StackAnimation {
    private static final String TAG = "StackAnimationLandscape";

    /**
     * Only Constructor.
     */
    public StackAnimationLandscape(Stack stack, float width, float height,
            float topBrowserControlsHeight, float borderFramePaddingTop,
            float borderFramePaddingTopOpaque, float borderFramePaddingLeft) {
        super(stack, width, height, topBrowserControlsHeight, borderFramePaddingTop,
                borderFramePaddingTopOpaque, borderFramePaddingLeft);
    }

    @Override
    protected ChromeAnimation<?> createViewMoreAnimatorSet(StackTab[] tabs, int selectedIndex) {
        ChromeAnimation<Animatable> set = new ChromeAnimation<Animatable>();

        if (selectedIndex + 1 >= tabs.length) return set;

        float offset = tabs[selectedIndex].getScrollOffset()
                - tabs[selectedIndex + 1].getScrollOffset()
                + (tabs[selectedIndex].getLayoutTab().getScaledContentWidth()
                               * VIEW_MORE_SIZE_RATIO);
        offset = Math.max(VIEW_MORE_MIN_SIZE, offset);
        for (int i = selectedIndex + 1; i < tabs.length; ++i) {
            addAnimation(set, tabs[i], StackTab.Property.SCROLL_OFFSET, tabs[i].getScrollOffset(),
                    tabs[i].getScrollOffset() + offset, VIEW_MORE_ANIMATION_DURATION, 0);
        }

        return set;
    }

    @Override
    protected ChromeAnimation<?> createReachTopAnimatorSet(StackTab[] tabs) {
        ChromeAnimation<Animatable> set = new ChromeAnimation<Animatable>();

        float screenTarget = 0.0f;
        for (int i = 0; i < tabs.length; ++i) {
            if (screenTarget >= tabs[i].getLayoutTab().getX()) {
                break;
            }
            addAnimation(set, tabs[i], StackTab.Property.SCROLL_OFFSET, tabs[i].getScrollOffset(),
                    mStack.screenToScroll(screenTarget), REACH_TOP_ANIMATION_DURATION, 0);
            screenTarget += tabs[i].getLayoutTab().getScaledContentWidth();
        }

        return set;
    }

    @Override
    protected ChromeAnimation<?> createNewTabOpenedAnimatorSet(
            StackTab[] tabs, int focusIndex, float discardRange) {
        return null;
    }

    @Override
    protected boolean isDefaultDiscardDirectionPositive() {
        return true;
    }

    @Override
    protected float getScreenPositionInScrollDirection(StackTab tab) {
        return tab.getLayoutTab().getX();
    }

    @Override
    protected void addTiltScrollAnimation(ChromeAnimation<Animatable> set, LayoutTab tab, float end,
            int duration, int startTime) {
        addAnimation(set, tab, LayoutTab.Property.TILTY, tab.getTiltY(), end, duration, startTime);
    }

    @Override
    protected float getScreenSizeInScrollDirection() {
        return mWidth;
    }
}
