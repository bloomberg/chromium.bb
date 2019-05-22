// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.Context;

import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.GridTabSwitcher;

/**
 * A {@link Layout} that shows all tabs in one grid view.
 */
public class GridTabSwitcherLayout
        extends Layout implements GridTabSwitcher.GridVisibilityObserver {
    private SceneLayer mSceneLayer = new SceneLayer();
    private GridTabSwitcher.GridController mGridController;
    // To force Toolbar finishes its animation when this Layout finished hiding.
    private final LayoutTab mDummyLayoutTab;

    public GridTabSwitcherLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, GridTabSwitcher.GridController gridController) {
        super(context, updateHost, renderHost);
        mDummyLayoutTab = createLayoutTab(Tab.INVALID_TAB_ID, false, false, false);
        mDummyLayoutTab.setShowToolbar(true);
        mGridController = gridController;
        mGridController.addOverviewModeObserver(this);
    }

    @Override
    public LayoutTab getLayoutTab(int id) {
        return mDummyLayoutTab;
    }

    @Override
    public void destroy() {
        if (mGridController != null) {
            mGridController.removeOverviewModeObserver(this);
        }
    }

    @Override
    public void show(long time, boolean animate) {
        super.show(time, animate);
        mGridController.showOverview(animate);
    }

    @Override
    public boolean onBackPressed() {
        if (mTabModelSelector.getCurrentModel().getCount() == 0) return false;

        if (mGridController.overviewVisible()) {
            mGridController.hideOverview(true);
            return true;
        }

        return false;
    }

    @Override
    protected EventFilter getEventFilter() {
        return null;
    }

    @Override
    protected SceneLayer getSceneLayer() {
        return mSceneLayer;
    }

    @Override
    public boolean handlesTabClosing() {
        return true;
    }

    @Override
    public boolean handlesTabCreating() {
        return true;
    }

    @Override
    public boolean handlesCloseAll() {
        return false;
    }

    // GridTabSwitcher.GridVisibilityObserver implementation.
    @Override
    public void onOverviewModeStartedShowing(boolean showToolbar) {}

    @Override
    public void onOverviewModeFinishedShowing() {
        doneShowing();
    }

    @Override
    public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
        startHiding(mTabModelSelector.getCurrentTabId(), false);
    }

    @Override
    public void onOverviewModeFinishedHiding() {}
}
