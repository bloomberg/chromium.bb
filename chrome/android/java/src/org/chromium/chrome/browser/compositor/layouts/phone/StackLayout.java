// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import android.content.Context;

import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

import java.util.ArrayList;

/**
 * Layout that displays all normal tabs in one stack and all incognito tabs in a second.
 */
public class StackLayout extends StackLayoutBase {
    // One stack for normal tabs and one stack for incognito tabs.
    private static final int NUM_STACKS = 2;

    public static final int NORMAL_STACK_INDEX = 0;
    public static final int INCOGNITO_STACK_INDEX = 1;

    /** Whether the current fling animation is the result of switching stacks. */
    private boolean mFlingFromModelChange;

    /**
     * @param context     The current Android's context.
     * @param updateHost  The {@link LayoutUpdateHost} view for this layout.
     * @param renderHost  The {@link LayoutRenderHost} view for this layout.
     */
    public StackLayout(Context context, LayoutUpdateHost updateHost, LayoutRenderHost renderHost) {
        super(context, updateHost, renderHost);
    }

    @Override
    public void setTabModelSelector(TabModelSelector modelSelector, TabContentManager manager) {
        super.setTabModelSelector(modelSelector, manager);
        ArrayList<TabList> tabLists = new ArrayList<TabList>();
        tabLists.add(modelSelector.getModel(false));
        tabLists.add(modelSelector.getModel(true));
        setTabLists(tabLists);
    }

    @Override
    protected int getTabStackIndex(int tabId) {
        if (tabId == Tab.INVALID_TAB_ID) {
            if (mTemporarySelectedStack != INVALID_STACK_INDEX) return mTemporarySelectedStack;

            return mTabModelSelector.isIncognitoSelected() ? INCOGNITO_STACK_INDEX
                                                           : NORMAL_STACK_INDEX;
        } else {
            return TabModelUtils.getTabById(mTabModelSelector.getModel(true), tabId) != null
                    ? INCOGNITO_STACK_INDEX
                    : NORMAL_STACK_INDEX;
        }
    }

    @Override
    public void onTabClosing(long time, int id) {
        super.onTabClosing(time, id);
        // Just in case we closed the last tab of a stack we need to trigger the overlap animation.
        startMarginAnimation(true);
        // Animate the stack to leave incognito mode.
        if (!mStacks.get(INCOGNITO_STACK_INDEX).isDisplayable()) onTabModelSwitched(false);
    }

    @Override
    public void onTabsAllClosing(long time, boolean incognito) {
        super.onTabsAllClosing(time, incognito);
        getTabStackAtIndex(incognito ? INCOGNITO_STACK_INDEX : NORMAL_STACK_INDEX)
                .tabsAllClosingEffect(time);
        // trigger the overlap animation.
        startMarginAnimation(true);
        // Animate the stack to leave incognito mode.
        if (!mStacks.get(INCOGNITO_STACK_INDEX).isDisplayable()) onTabModelSwitched(false);
    }

    @Override
    public void onTabClosureCancelled(long time, int id, boolean incognito) {
        super.onTabClosureCancelled(time, id, incognito);
        getTabStackAtIndex(incognito ? INCOGNITO_STACK_INDEX : NORMAL_STACK_INDEX)
                .undoClosure(time, id);
    }

    @Override
    public void onTabCreated(long time, int id, int tabIndex, int sourceId, boolean newIsIncognito,
            boolean background, float originX, float originY) {
        super.onTabCreated(
                time, id, tabIndex, sourceId, newIsIncognito, background, originX, originY);
        onTabModelSwitched(newIsIncognito);
    }

    @Override
    public void onTabModelSwitched(boolean toIncognitoTabModel) {
        flingStacks(toIncognitoTabModel ? INCOGNITO_STACK_INDEX : NORMAL_STACK_INDEX);
        mFlingFromModelChange = true;
    }

    @Override
    protected void onAnimationFinished() {
        super.onAnimationFinished();
        mFlingFromModelChange = false;
        if (mTemporarySelectedStack != INVALID_STACK_INDEX) {
            mTabModelSelector.selectModel(mTemporarySelectedStack == INCOGNITO_STACK_INDEX);
            mTemporarySelectedStack = INVALID_STACK_INDEX;
        }
    }

    @Override
    protected int getMinRenderedScrollOffset() {
        if (mStacks.get(INCOGNITO_STACK_INDEX).isDisplayable() || mFlingFromModelChange) return -1;
        return 0;
    }

    @Override
    public void uiRequestingCloseTab(long time, int id) {
        super.uiRequestingCloseTab(time, id);

        int incognitoCount = mTabModelSelector.getModel(true).getCount();
        TabModel model = mTabModelSelector.getModelForTabId(id);
        if (model != null && model.isIncognito()) incognitoCount--;
        boolean incognitoVisible = incognitoCount > 0;

        // Make sure we show/hide both stacks depending on which tab we're closing.
        startMarginAnimation(true, incognitoVisible);
        if (!incognitoVisible) onTabModelSwitched(false);
    }

    @Override
    protected @SwipeMode int computeInputMode(long time, float x, float y, float dx, float dy) {
        if (mStacks.size() == 2 && !mStacks.get(1).isDisplayable()) return SWIPE_MODE_SEND_TO_STACK;
        return super.computeInputMode(time, x, y, dx, dy);
    }
}
