// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Rect;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

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
public class TabGridDialogCoordinator implements TabGridDialogMediator.ResetHandler {
    private final String mComponentName;
    private final Context mContext;
    private final TabListCoordinator mTabListCoordinator;
    private final TabGridDialogMediator mMediator;
    private final PropertyModel mToolbarPropertyModel;
    private final TabGridSheetToolbarCoordinator mToolbarCoordinator;
    private TabGridDialogParent mParentLayout;

    TabGridDialogCoordinator(Context context, TabModelSelector tabModelSelector,
            TabContentManager tabContentManager, TabCreatorManager tabCreatorManager,
            CompositorViewHolder compositorViewHolder,
            TabSwitcherMediator.ResetHandler resetHandler,
            TabListMediator.GridCardOnClickListenerProvider gridCardOnClickListenerProvider,
            TabGridDialogMediator.AnimationParamsProvider animationParamsProvider) {
        mContext = context;

        mComponentName = animationParamsProvider == null ? "TabGridDialogFromStrip"
                                                         : "TabGridDialogInSwitcher";

        mToolbarPropertyModel = new PropertyModel(TabGridSheetProperties.ALL_KEYS);

        mMediator =
                new TabGridDialogMediator(context, this, mToolbarPropertyModel, tabModelSelector,
                        tabCreatorManager, resetHandler, animationParamsProvider, mComponentName);

        mTabListCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.GRID, context,
                tabModelSelector, tabContentManager::getTabThumbnailWithCallback, null, false, null,
                gridCardOnClickListenerProvider, mMediator.getTabGridDialogHandler(), null, null,
                compositorViewHolder, null, false, mComponentName);

        mParentLayout = new TabGridDialogParent(context, compositorViewHolder);

        TabListRecyclerView recyclerView = mTabListCoordinator.getContainerView();
        mToolbarCoordinator = new TabGridSheetToolbarCoordinator(
                mContext, recyclerView, mToolbarPropertyModel, mParentLayout);
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mTabListCoordinator.destroy();
        mMediator.destroy();
        mToolbarCoordinator.destroy();
        mParentLayout.destroy();
    }

    private void updateDialogContent(List<Tab> tabs) {
        if (tabs != null) {
            mMediator.onReset(tabs.get(0).getId());
        } else {
            mMediator.onReset(null);
        }
    }

    boolean isVisible() {
        return mMediator.isVisible();
    }

    @NonNull
    Rect getGlobalLocationOfCurrentThumbnail() {
        mTabListCoordinator.updateThumbnailLocation();
        Rect thumbnail = mTabListCoordinator.getThumbnailLocationOfCurrentTab();
        Rect recyclerViewLocation = mTabListCoordinator.getRecyclerViewLocation();
        thumbnail.offset(recyclerViewLocation.left, recyclerViewLocation.top);
        return thumbnail;
    }

    TabGridDialogMediator.ResetHandler getResetHandler() {
        return this;
    }

    @Override
    public void resetWithListOfTabs(@Nullable List<Tab> tabs) {
        mTabListCoordinator.resetWithListOfTabs(tabs);
        updateDialogContent(tabs);
    }

    @Override
    public void hideDialog(boolean showAnimation) {
        mMediator.hideDialog(showAnimation);
    }
}
