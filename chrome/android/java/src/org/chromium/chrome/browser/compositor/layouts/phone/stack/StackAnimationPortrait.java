// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone.stack;

import static org.chromium.chrome.browser.compositor.layouts.ChromeAnimation.AnimatableAnimation.addAnimation;

import org.chromium.chrome.browser.compositor.layouts.ChromeAnimation;
import org.chromium.chrome.browser.compositor.layouts.ChromeAnimation.Animatable;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.ui.base.LocalizationUtils;

class StackAnimationPortrait extends StackAnimation {
    /**
     * Only Constructor.
     */
    public StackAnimationPortrait(Stack stack, float width, float height,
            float topBrowserControlsHeight, float borderFramePaddingTop,
            float borderFramePaddingTopOpaque, float borderFramePaddingLeft) {
        super(stack, width, height, topBrowserControlsHeight, borderFramePaddingTop,
                borderFramePaddingTopOpaque, borderFramePaddingLeft);
    }

    @Override
    protected ChromeAnimation<?> createEnterStackAnimatorSet(
            StackTab[] tabs, int focusIndex, int spacing) {
        ChromeAnimation<Animatable> set = new ChromeAnimation<Animatable>();
        final float initialScrollOffset = mStack.screenToScroll(0);

        float trailingScrollOffset = 0.f;
        if (focusIndex >= 0 && focusIndex < tabs.length - 1) {
            final float focusOffset = tabs[focusIndex].getScrollOffset();
            final float nextOffset = tabs[focusIndex + 1].getScrollOffset();
            final float topSpacing = focusIndex == 0 ? spacing : 0.f;
            final float extraSpace = tabs[focusIndex].getLayoutTab().getScaledContentHeight()
                    * ENTER_STACK_SIZE_RATIO;
            trailingScrollOffset = Math.max(focusOffset - nextOffset + topSpacing + extraSpace, 0);
        }

        for (int i = 0; i < tabs.length; ++i) {
            StackTab tab = tabs[i];

            tab.resetOffset();
            tab.setScale(mStack.getScaleAmount());
            tab.setAlpha(1.f);
            tab.getLayoutTab().setToolbarAlpha(0.f);
            tab.getLayoutTab().setBorderScale(1.f);

            float scrollOffset = mStack.screenToScroll(i * spacing);

            if (i < focusIndex) {
                tab.getLayoutTab().setMaxContentHeight(mStack.getMaxTabHeight());
                addAnimation(set, tab, StackTab.Property.SCROLL_OFFSET, initialScrollOffset,
                        scrollOffset, ENTER_STACK_ANIMATION_DURATION, 0);
            } else if (i > focusIndex) {
                tab.getLayoutTab().setMaxContentHeight(mStack.getMaxTabHeight());
                tab.setScrollOffset(scrollOffset + trailingScrollOffset);
                addAnimation(set, tab, StackTab.Property.Y_IN_STACK_OFFSET, mHeight, 0,
                        ENTER_STACK_ANIMATION_DURATION, 0);
            } else {
                tab.setScrollOffset(scrollOffset);

                addAnimation(set, tab.getLayoutTab(), LayoutTab.Property.MAX_CONTENT_HEIGHT,
                        tab.getLayoutTab().getUnclampedOriginalContentHeight(),
                        mStack.getMaxTabHeight(), ENTER_STACK_ANIMATION_DURATION,
                        ENTER_STACK_RESIZE_DELAY);
                addAnimation(set, tab, StackTab.Property.Y_IN_STACK_INFLUENCE, 0.0f, 1.0f,
                        ENTER_STACK_BORDER_ALPHA_DURATION, 0);
                addAnimation(set, tab, StackTab.Property.SCALE, 1.0f, mStack.getScaleAmount(),
                        ENTER_STACK_BORDER_ALPHA_DURATION, 0);
                addAnimation(set, tab.getLayoutTab(), LayoutTab.Property.TOOLBAR_ALPHA, 1.f, 0.f,
                        ENTER_STACK_BORDER_ALPHA_DURATION, ENTER_STACK_TOOLBAR_ALPHA_DELAY);
                addAnimation(set, tab.getLayoutTab(), LayoutTab.Property.TOOLBAR_Y_OFFSET, 0.f,
                        getToolbarOffsetToLineUpWithBorder(), ENTER_STACK_BORDER_ALPHA_DURATION,
                        TAB_FOCUSED_TOOLBAR_ALPHA_DELAY);
                addAnimation(set, tab.getLayoutTab(), LayoutTab.Property.SIDE_BORDER_SCALE, 0.f,
                        1.f, ENTER_STACK_BORDER_ALPHA_DURATION, TAB_FOCUSED_TOOLBAR_ALPHA_DELAY);

                tab.setYOutOfStack(getStaticTabPosition());
            }
        }

        return set;
    }

    @Override
    protected ChromeAnimation<?> createTabFocusedAnimatorSet(
            StackTab[] tabs, int focusIndex, int spacing) {
        ChromeAnimation<Animatable> set = new ChromeAnimation<Animatable>();
        for (int i = 0; i < tabs.length; ++i) {
            StackTab tab = tabs[i];
            LayoutTab layoutTab = tab.getLayoutTab();

            addTiltScrollAnimation(
                    set, layoutTab, 0.0f, StackAnimation.TAB_FOCUSED_ANIMATION_DURATION, 0);
            addAnimation(set, tab, StackTab.Property.DISCARD_AMOUNT, tab.getDiscardAmount(), 0.0f,
                    TAB_FOCUSED_ANIMATION_DURATION, 0);

            if (i < focusIndex) {
                // For tabs above the focused tab move them up to 0.
                addAnimation(set, tab, StackTab.Property.SCROLL_OFFSET, tab.getScrollOffset(),
                        tab.getScrollOffset() - mHeight - spacing, TAB_FOCUSED_ANIMATION_DURATION,
                        0);
            } else if (i > focusIndex) {
                // We also need to animate the Y Translation to move them down
                // off the screen.
                float coveringTabPosition = layoutTab.getY();
                float distanceToBorder = MathUtils.clamp(mHeight - coveringTabPosition, 0, mHeight);
                float delay = TAB_FOCUSED_MAX_DELAY * distanceToBorder / mHeight;
                addAnimation(set, tab, StackTab.Property.Y_IN_STACK_OFFSET, tab.getYInStackOffset(),
                        tab.getYInStackOffset() + mHeight,
                        (TAB_FOCUSED_ANIMATION_DURATION - (long) delay), (long) delay);
            } else {
                // This is the focused tab.  We need to scale it back to
                // 1.0f, move it to the top of the screen, and animate the
                // YTranslation so that it looks like it is zooming into the
                // full screen view.
                tab.setXOutOfStack(0.0f);
                tab.setYOutOfStack(0.0f);
                layoutTab.setBorderScale(1.f);

                addAnimation(set, tab, StackTab.Property.SCROLL_OFFSET, tab.getScrollOffset(),
                        Math.max(0.0f, tab.getScrollOffset() - mWidth - spacing),
                        TAB_FOCUSED_ANIMATION_DURATION, 0);
                addAnimation(set, tab, StackTab.Property.SCALE, tab.getScale(), 1.0f,
                        TAB_FOCUSED_ANIMATION_DURATION, 0);
                addAnimation(set, tab, StackTab.Property.Y_IN_STACK_INFLUENCE,
                        tab.getYInStackInfluence(), 0.0f, TAB_FOCUSED_Y_STACK_DURATION, 0);
                addAnimation(set, tab.getLayoutTab(), LayoutTab.Property.MAX_CONTENT_HEIGHT,
                        tab.getLayoutTab().getMaxContentHeight(),
                        tab.getLayoutTab().getUnclampedOriginalContentHeight(),
                        TAB_FOCUSED_ANIMATION_DURATION, 0);
                tab.setYOutOfStack(getStaticTabPosition());
                if (layoutTab.shouldStall()) {
                    addAnimation(set, layoutTab, LayoutTab.Property.SATURATION, 1.0f, 0.0f,
                            TAB_FOCUSED_BORDER_ALPHA_DURATION, TAB_FOCUSED_BORDER_ALPHA_DELAY);
                }
                addAnimation(set, tab.getLayoutTab(), LayoutTab.Property.TOOLBAR_ALPHA,
                        layoutTab.getToolbarAlpha(), 1.f, TAB_FOCUSED_TOOLBAR_ALPHA_DURATION,
                        TAB_FOCUSED_TOOLBAR_ALPHA_DELAY);
                addAnimation(set, tab.getLayoutTab(), LayoutTab.Property.TOOLBAR_Y_OFFSET,
                        getToolbarOffsetToLineUpWithBorder(), 0.f,
                        TAB_FOCUSED_TOOLBAR_ALPHA_DURATION, TAB_FOCUSED_TOOLBAR_ALPHA_DELAY);
                addAnimation(set, tab.getLayoutTab(), LayoutTab.Property.SIDE_BORDER_SCALE, 1.f,
                        0.f, TAB_FOCUSED_TOOLBAR_ALPHA_DURATION, TAB_FOCUSED_TOOLBAR_ALPHA_DELAY);
            }
        }

        return set;
    }

    @Override
    protected ChromeAnimation<?> createViewMoreAnimatorSet(StackTab[] tabs, int selectedIndex) {
        ChromeAnimation<Animatable> set = new ChromeAnimation<Animatable>();

        if (selectedIndex + 1 >= tabs.length) return set;

        float offset = tabs[selectedIndex].getScrollOffset()
                - tabs[selectedIndex + 1].getScrollOffset()
                + (tabs[selectedIndex].getLayoutTab().getScaledContentHeight()
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
            if (screenTarget >= tabs[i].getLayoutTab().getY()) {
                break;
            }
            addAnimation(set, tabs[i], StackTab.Property.SCROLL_OFFSET, tabs[i].getScrollOffset(),
                    mStack.screenToScroll(screenTarget), REACH_TOP_ANIMATION_DURATION, 0);
            screenTarget += tabs[i].getLayoutTab().getScaledContentHeight();
        }

        return set;
    }

    @Override
    protected boolean isDefaultDiscardDirectionPositive() {
        // On clicking the close button, discard the tab to the right on LTR, to the left on RTL.
        return !LocalizationUtils.isLayoutRtl();
    }

    @Override
    protected float getScreenPositionInScrollDirection(StackTab tab) {
        return tab.getLayoutTab().getY();
    }

    @Override
    protected void addTiltScrollAnimation(ChromeAnimation<Animatable> set, LayoutTab tab, float end,
            int duration, int startTime) {
        addAnimation(set, tab, LayoutTab.Property.TILTX, tab.getTiltX(), end, duration, startTime);
    }

    @Override
    protected float getScreenSizeInScrollDirection() {
        return mHeight;
    }
}
