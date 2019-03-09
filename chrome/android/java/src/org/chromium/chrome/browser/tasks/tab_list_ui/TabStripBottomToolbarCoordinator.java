// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A coordinator for TabStripBottomToolbar component. Manages the communication with
 * {@link TabListCoordinator}, {@link BottomTabGridCoordinator}, and
 * {@link TabStripToolbarCoordinator}, as well as the life-cycle of shared component objects.
 */
public class TabStripBottomToolbarCoordinator
        implements Destroyable, TabStripBottomToolbarMediator.ResetHandler {
    private final Context mContext;
    private final PropertyModel mTabStripToolbarModel;
    private BottomTabGridCoordinator mBottomTabGridCoordinator;
    private TabListCoordinator mTabStripCoordinator;
    private TabStripBottomToolbarMediator mMediator;
    private TabStripToolbarCoordinator mTabStripToolbarCoordinator;

    /**
     * Creates a new {@link TabStripBottomToolbarCoordinator}
     */
    public TabStripBottomToolbarCoordinator(ViewGroup parentView) {
        mContext = parentView.getContext();
        mTabStripToolbarModel = new PropertyModel(TabStripToolbarViewProperties.ALL_KEYS);

        mTabStripToolbarCoordinator =
                new TabStripToolbarCoordinator(mContext, parentView, mTabStripToolbarModel);
    }

    /**
     * Handle any initialization that occurs once native has been loaded.
     */
    public void initializeWithNative(TabModelSelector tabModelSelector,
            TabContentManager tabContentManager, TabCreatorManager tabCreatorManager,
            BottomSheetController bottomSheetController) {
        mTabStripCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.STRIP,
                mContext, tabModelSelector, tabContentManager,
                mTabStripToolbarCoordinator.getTabListContainerView(), true);

        mBottomTabGridCoordinator = new BottomTabGridCoordinator(mContext, bottomSheetController,
                tabModelSelector, tabContentManager, tabCreatorManager);

        mMediator = new TabStripBottomToolbarMediator(
                this, mTabStripToolbarModel, tabModelSelector, tabCreatorManager);
    }

    /**
     * Handles a reset event originated from {@link TabStripBottomToolbarMediator}
     * when the bottom sheet is collapsed.
     *
     * @param tabs List of Tabs to reset.
     */
    @Override
    public void resetStripWithListOfTabs(List<Tab> tabs) {
        mTabStripCoordinator.resetWithListOfTabs(tabs);
    }

    /**
     * Handles a reset event originated from {@link TabStripBottomToolbarMediator}
     * when the bottom sheet is expanded.
     *
     * @param tabs List of Tabs to reset.
     */
    @Override
    public void resetSheetWithListOfTabs(List<Tab> tabs) {
        mBottomTabGridCoordinator.resetWithListOfTabs(tabs);
    }

    /**
     * Destroy any members that needs clean up.
     */
    @Override
    public void destroy() {
        mTabStripCoordinator.destroy();
        mBottomTabGridCoordinator.destroy();
        mMediator.destroy();
    }
}
