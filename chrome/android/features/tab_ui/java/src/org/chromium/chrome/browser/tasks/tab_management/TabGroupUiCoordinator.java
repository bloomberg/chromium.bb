// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A coordinator for TabGroupUi component. Manages the communication with
 * {@link TabListCoordinator}, {@link TabGridSheetCoordinator}, and
 * {@link TabStripToolbarCoordinator}, as well as the life-cycle of shared component objects.
 */
public class TabGroupUiCoordinator
        implements Destroyable, TabGroupUiMediator.ResetHandler, TabGroupUi {
    private final Context mContext;
    private final PropertyModel mTabStripToolbarModel;
    private TabGridSheetCoordinator mTabGridSheetCoordinator;
    private TabListCoordinator mTabStripCoordinator;
    private TabGroupUiMediator mMediator;
    private TabStripToolbarCoordinator mTabStripToolbarCoordinator;

    /**
     * Creates a new {@link TabGroupUiCoordinator}
     */
    public TabGroupUiCoordinator(ViewGroup parentView) {
        mContext = parentView.getContext();
        mTabStripToolbarModel = new PropertyModel(TabStripToolbarViewProperties.ALL_KEYS);

        mTabStripToolbarCoordinator =
                new TabStripToolbarCoordinator(mContext, parentView, mTabStripToolbarModel);
    }

    /**
     * Handle any initialization that occurs once native has been loaded.
     */
    @Override
    public void initializeWithNative(ChromeActivity activity,
            BottomControlsCoordinator.BottomControlsVisibilityController visibilityController) {
        assert activity instanceof ChromeTabbedActivity;

        TabModelSelector tabModelSelector = activity.getTabModelSelector();
        TabContentManager tabContentManager = activity.getTabContentManager();
        mTabStripCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.STRIP,
                mContext, tabModelSelector, tabContentManager,
                mTabStripToolbarCoordinator.getTabListContainerView(), true);

        mTabGridSheetCoordinator = new TabGridSheetCoordinator(mContext,
                activity.getBottomSheetController(), tabModelSelector, tabContentManager, activity);

        mMediator = new TabGroupUiMediator(visibilityController, this, mTabStripToolbarModel,
                tabModelSelector, activity,
                ((ChromeTabbedActivity) activity).getOverviewModeBehavior());
    }

    /**
     * Handles a reset event originated from {@link TabGroupUiMediator}
     * when the bottom sheet is collapsed.
     *
     * @param tabs List of Tabs to reset.
     */
    @Override
    public void resetStripWithListOfTabs(List<Tab> tabs) {
        mTabStripCoordinator.resetWithListOfTabs(tabs);
    }

    /**
     * Handles a reset event originated from {@link TabGroupUiMediator}
     * when the bottom sheet is expanded.
     *
     * @param tabs List of Tabs to reset.
     */
    @Override
    public void resetSheetWithListOfTabs(List<Tab> tabs) {
        mTabGridSheetCoordinator.resetWithListOfTabs(tabs);
    }

    /**
     * Destroy any members that needs clean up.
     */
    @Override
    public void destroy() {
        mTabStripCoordinator.destroy();
        mTabGridSheetCoordinator.destroy();
        mMediator.destroy();
    }
}
