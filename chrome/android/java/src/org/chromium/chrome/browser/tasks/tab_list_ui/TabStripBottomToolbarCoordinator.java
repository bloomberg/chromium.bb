// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A coordinator for BottomTabStrip component. Manages the communication with
 * {@link TabListCoordinator} & @{link BottomTabGridCoordinator} as well as the
 * life-cycle of shared component objects.
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
    public TabStripBottomToolbarCoordinator(Context context, ViewGroup parentView) {
        mContext = context;
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
     * when the bottom sheet is collaped.
     *
     * @param tabModel current {@link TabModel} instance.
     */
    @Override
    public void resetStripWithTabModel(TabModel tabModel) {
        mTabStripCoordinator.resetWithTabModel(tabModel);
    }

    /**
     * Handles a reset event originated from {@link TabStripBottomToolbarMediator}
     * when the bottom sheet is expanded and the component.
     *
     * @param tabModel current {@link TabModel} instance.
     */
    @Override
    public void resetSheetWithTabModel(TabModel tabModel) {
        mBottomTabGridCoordinator.resetWithTabModel(tabModel);
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
