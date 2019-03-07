// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupUtils;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A coordinator for BottomTabGrid component. Manages the communication with
 * {@link TabListCoordinator} as well as the life-cycle of shared component
 * objects.
 */
@ActivityScope
public class BottomTabGridCoordinator implements Destroyable {
    private final Context mContext;
    private final TabListCoordinator mTabGridCoordinator;
    private final BottomTabGridMediator mMediator;
    private BottomTabGridSheetContent mBottomSheetContent;
    private BottomTabGridSheetToolbarCoordinator mToolbarCoordinator;
    private final PropertyModel mToolbarPropertyModel;

    BottomTabGridCoordinator(Context context, BottomSheetController bottomSheetController,
            TabModelSelector tabModelSelector, TabContentManager tabContentManager,
            TabCreatorManager tabCreatorManager) {
        mContext = context;

        mToolbarPropertyModel = new PropertyModel(BottomTabGridSheetProperties.ALL_KEYS);

        mTabGridCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.GRID, context,
                tabModelSelector, tabContentManager, bottomSheetController.getBottomSheet(), false);

        mMediator =
                new BottomTabGridMediator(mContext, bottomSheetController, this::resetWithTabModel,
                        mToolbarPropertyModel, tabModelSelector, tabCreatorManager);
        startObservingForCreationIPH();
    }

    /**
     * Destroy any members that needs clean up.
     */
    @Override
    public void destroy() {
        mTabGridCoordinator.destroy();
        mMediator.destroy();

        if (mBottomSheetContent != null) {
            mBottomSheetContent.destroy();
        }

        if (mToolbarCoordinator != null) {
            mToolbarCoordinator.destroy();
        }
    }

    /**
     * Updates tabs list through {@link TabListCoordinator} with given tab model and
     * calls onReset() on {@link BottomTabGridMediator}
     */
    public void resetWithTabModel(TabModel tabModel) {
        mTabGridCoordinator.resetWithTabModel(tabModel);
        updateBottomSheetContent(tabModel);
        mMediator.onReset(mBottomSheetContent);
    }

    private void updateBottomSheetContent(TabModel tabModel) {
        if (tabModel != null) {
            // create bottom sheet content
            mToolbarCoordinator = new BottomTabGridSheetToolbarCoordinator(
                    mContext, mTabGridCoordinator.getContainerView(), mToolbarPropertyModel);
            mBottomSheetContent = new BottomTabGridSheetContent(
                    mTabGridCoordinator.getContainerView(), mToolbarCoordinator.getView());
        } else {
            if (mBottomSheetContent != null) {
                mBottomSheetContent.destroy();
                mBottomSheetContent = null;
            }

            if (mToolbarCoordinator != null) {
                mToolbarCoordinator.destroy();
            }
        }
    }

    private void startObservingForCreationIPH() {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(activity instanceof ChromeTabbedActivity)) return;

        TabGroupUtils.startObservingForTabGroupsIPH(
                ((ChromeTabbedActivity) activity).getTabModelSelector());
    }
}
