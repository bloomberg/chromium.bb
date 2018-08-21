// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone.stack;

import static org.chromium.chrome.browser.compositor.layouts.ChromeAnimation.AnimatableAnimation.addAnimation;
import static org.chromium.chrome.browser.compositor.layouts.components.LayoutTab.Property.MAX_CONTENT_HEIGHT;
import static org.chromium.chrome.browser.compositor.layouts.components.LayoutTab.Property.SATURATION;
import static org.chromium.chrome.browser.compositor.layouts.components.LayoutTab.Property.TOOLBAR_ALPHA;
import static org.chromium.chrome.browser.compositor.layouts.components.LayoutTab.Property.TOOLBAR_Y_OFFSET;
import static org.chromium.chrome.browser.compositor.layouts.phone.stack.StackTab.Property.DISCARD_AMOUNT;
import static org.chromium.chrome.browser.compositor.layouts.phone.stack.StackTab.Property.SCALE;
import static org.chromium.chrome.browser.compositor.layouts.phone.stack.StackTab.Property.SCROLL_OFFSET;
import static org.chromium.chrome.browser.compositor.layouts.phone.stack.StackTab.Property.Y_IN_STACK_INFLUENCE;

import org.chromium.chrome.browser.compositor.layouts.ChromeAnimation;
import org.chromium.chrome.browser.compositor.layouts.ChromeAnimation.Animatable;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;

class StackAnimationLandscapeNonOverlapping extends StackAnimationLandscape {
    public StackAnimationLandscapeNonOverlapping(Stack stack, float width, float height,
            float topBrowserControlsHeight, float borderFramePaddingTop,
            float borderFramePaddingTopOpaque, float borderFramePaddingLeft) {
        super(stack, width, height, topBrowserControlsHeight, borderFramePaddingTop,
                borderFramePaddingTopOpaque, borderFramePaddingLeft);
    }

    @Override
    protected ChromeAnimation<?> createEnterStackAnimatorSet(
            Stack stack, StackTab[] tabs, int focusIndex, int spacing) {
        ChromeAnimation<Animatable> set = new ChromeAnimation<Animatable>();
        final float initialScrollOffset = mStack.screenToScroll(0);

        for (int i = 0; i < tabs.length; ++i) {
            StackTab tab = tabs[i];

            tab.resetOffset();
            tab.setScale(mStack.getScaleAmount());
            tab.setAlpha(1.f);
            tab.getLayoutTab().setToolbarAlpha(0.f);
            tab.getLayoutTab().setBorderScale(1.f);

            final float scrollOffset = i * spacing;

            addAnimation(set, tab.getLayoutTab(), MAX_CONTENT_HEIGHT,
                    tab.getLayoutTab().getUnclampedOriginalContentHeight(),
                    mStack.getMaxTabHeight(), ENTER_STACK_ANIMATION_DURATION, 0);

            addAnimation(set, tab, SCALE, 1.0f, mStack.getScaleAmount(),
                    ENTER_STACK_ANIMATION_DURATION, 0);

            tab.setScrollOffset(i * spacing / mStack.getScaleAmount());

            addAnimation(set, tab, SCROLL_OFFSET, tab.getScrollOffset(), scrollOffset,
                    ENTER_STACK_ANIMATION_DURATION, 0);

            if (i == focusIndex) {
                NonOverlappingStack nonOverlappingStack = (NonOverlappingStack) stack;
                addAnimation(set, nonOverlappingStack, Stack.Property.SCROLL_OFFSET,
                        (-focusIndex * spacing) / mStack.getScaleAmount(), (-focusIndex * spacing),
                        ENTER_STACK_ANIMATION_DURATION, 0);

                addAnimation(set, tab.getLayoutTab(), TOOLBAR_ALPHA, 1.f, 0.f,
                        ENTER_STACK_ANIMATION_DURATION, 0);
                addAnimation(set, tab.getLayoutTab(), TOOLBAR_Y_OFFSET, 0.f,
                        getToolbarOffsetToLineUpWithBorder(), ENTER_STACK_ANIMATION_DURATION, 0);
            }
        }

        return set;
    }

    @Override
    protected ChromeAnimation<?> createTabFocusedAnimatorSet(
            Stack stack, StackTab[] tabs, int focusIndex, int spacing) {
        ChromeAnimation<Animatable> set = new ChromeAnimation<Animatable>();

        for (int i = 0; i < tabs.length; ++i) {
            StackTab tab = tabs[i];
            LayoutTab layoutTab = tab.getLayoutTab();

            addTiltScrollAnimation(set, layoutTab, 0.0f, TAB_FOCUSED_ANIMATION_DURATION, 0);
            addAnimation(set, tab, DISCARD_AMOUNT, tab.getDiscardAmount(), 0.0f,
                    TAB_FOCUSED_ANIMATION_DURATION, 0);

            addAnimation(set, tab, SCALE, tab.getScale(), 1.0f, TAB_FOCUSED_ANIMATION_DURATION, 0);

            addAnimation(set, tab, SCROLL_OFFSET, tab.getScrollOffset(),
                    i * spacing / tab.getScale(), TAB_FOCUSED_ANIMATION_DURATION, 0);

            addAnimation(set, tab, Y_IN_STACK_INFLUENCE, tab.getYInStackInfluence(), 0.0f,
                    TAB_FOCUSED_ANIMATION_DURATION, 0);
            addAnimation(set, tab.getLayoutTab(), TOOLBAR_Y_OFFSET,
                    getToolbarOffsetToLineUpWithBorder(), 0.f, TAB_FOCUSED_ANIMATION_DURATION,
                    TAB_FOCUSED_TOOLBAR_ALPHA_DELAY);

            tab.setYOutOfStack(0.0f);
            tab.setYOutOfStack(getStaticTabPosition());
            layoutTab.setBorderScale(1.f);

            if (layoutTab.shouldStall()) {
                addAnimation(set, layoutTab, SATURATION, 1.0f, 0.0f,
                        TAB_FOCUSED_BORDER_ALPHA_DURATION, TAB_FOCUSED_BORDER_ALPHA_DELAY);
            }
            addAnimation(set, tab.getLayoutTab(), TOOLBAR_ALPHA, layoutTab.getToolbarAlpha(), 1.f,
                    TAB_FOCUSED_TOOLBAR_ALPHA_DURATION, TAB_FOCUSED_TOOLBAR_ALPHA_DELAY);

            if (i == focusIndex) {
                NonOverlappingStack nonOverlappingStack = (NonOverlappingStack) stack;
                addAnimation(set, nonOverlappingStack, Stack.Property.SCROLL_OFFSET,
                        stack.getScrollOffset(), (-focusIndex * spacing) / tab.getScale(),
                        TAB_FOCUSED_ANIMATION_DURATION, 0);
            }
        }

        return set;
    }
}
