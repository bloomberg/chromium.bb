// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.ViewGroup;

import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A coordinator for TabGridDialog component. Manages the communication with
 * {@link TabListCoordinator} as well as the life-cycle of shared component
 * objects.
 */
public class TabGridDialogCoordinator {
    final static String COMPONENT_NAME = "TabGridDialog";
    private final Context mContext;
    private final TabListCoordinator mTabListCoordinator;
    private final TabGridDialogMediator mMediator;
    private final PropertyModel mToolbarPropertyModel;
    private TabGridSheetToolbarCoordinator mToolbarCoordinator;
    private ViewGroup mParentView;
    private TabGridDialogParent mParentLayout;

    TabGridDialogCoordinator(Context context, TabModelSelector tabModelSelector,
            TabContentManager tabContentManager, TabCreatorManager tabCreatorManager,
            CompositorViewHolder compositorViewHolder,
            GridTabSwitcherMediator.ResetHandler resetHandler) {
        mContext = context;

        mToolbarPropertyModel = new PropertyModel(TabGridSheetProperties.ALL_KEYS);

        mTabListCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.GRID, context,
                tabModelSelector, tabContentManager::getTabThumbnailWithCallback, null, false, null,
                null, compositorViewHolder, false, COMPONENT_NAME);

        mMediator = new TabGridDialogMediator(context, this::resetWithListOfTabs,
                mToolbarPropertyModel, tabModelSelector, tabCreatorManager, resetHandler);

        mParentView = compositorViewHolder;

        mParentLayout = new TabGridDialogParent(context);
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mTabListCoordinator.destroy();
        mMediator.destroy();
    }

    private void updateDialogContent(List<Tab> tabs) {
        if (tabs != null) {
            TabListRecyclerView recyclerView = mTabListCoordinator.getContainerView();
            mToolbarCoordinator = new TabGridSheetToolbarCoordinator(
                    mContext, recyclerView, mToolbarPropertyModel, mParentView, mParentLayout);
        } else {
            if (mToolbarCoordinator != null) {
                mToolbarCoordinator.destroy();
            }
        }
    }

    TabGridDialogMediator.ResetHandler getResetHandler() {
        return this::resetWithListOfTabs;
    }

    public void resetWithListOfTabs(@Nullable List<Tab> tabs) {
        mTabListCoordinator.resetWithListOfTabs(tabs);
        updateDialogContent(tabs);
        mMediator.onReset(tabs == null ? null : tabs.get(0).getId());
    }
}
