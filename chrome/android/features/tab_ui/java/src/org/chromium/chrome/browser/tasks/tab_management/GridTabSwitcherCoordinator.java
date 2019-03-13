// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeController;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * Parent coordinator that is responsible for showing a grid of tabs for the main TabSwitcher UI.
 */
public class GridTabSwitcherCoordinator
        implements Destroyable, GridTabSwitcher, GridTabSwitcherMediator.ResetHandler {
    private final PropertyModelChangeProcessor mContainerViewChangeProcessor;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final TabListCoordinator mTabGridCoordinator;
    private final GridTabSwitcherMediator mMediator;

    public GridTabSwitcherCoordinator(Context context,
            ActivityLifecycleDispatcher lifecycleDispatcher, ToolbarManager toolbarManager,
            TabModelSelector tabModelSelector, TabContentManager tabContentManager,
            CompositorViewHolder compositorViewHolder, ChromeFullscreenManager fullscreenManager) {
        PropertyModel containerViewModel = new PropertyModel(TabListContainerProperties.ALL_KEYS);

        mTabGridCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.GRID, context,
                tabModelSelector, tabContentManager, compositorViewHolder, true);

        mContainerViewChangeProcessor = PropertyModelChangeProcessor.create(containerViewModel,
                mTabGridCoordinator.getContainerView(), TabGridContainerViewBinder::bind);

        mMediator = new GridTabSwitcherMediator(
                this, containerViewModel, tabModelSelector, fullscreenManager);

        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
    }

    /**
     * @return OverviewModeController implementation that will can be used for controlling
     *         OverviewMode changes.
     */
    @Override
    public OverviewModeController getOverviewModeController() {
        return mMediator;
    }

    /**
     * Reset the tab grid with the given {@link TabModel}. Can be null.
     * @param tabModel The current {@link TabModel} to show the tabs for in the grid.
     */
    @Override
    public void resetWithTabModel(TabModel tabModel) {
        List<Tab> tabs = null;
        if (tabModel != null) {
            tabs = new ArrayList<>();
            for (int i = 0; i < tabModel.getCount(); i++) {
                tabs.add(tabModel.getTabAt(i));
            }
        }
        mTabGridCoordinator.resetWithListOfTabs(tabs);
    }

    @Override
    public void destroy() {
        mTabGridCoordinator.destroy();
        mContainerViewChangeProcessor.destroy();
        mMediator.destroy();
        mLifecycleDispatcher.unregister(this);
    }
}
