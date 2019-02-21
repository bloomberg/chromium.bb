// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.ACTIVITY_CONTEXT;

import android.content.Context;

import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

import javax.inject.Inject;
import javax.inject.Named;

/**
 * A coordinator for BottomTabGrid component. Manages the communication
 * with {@link TabListCoordinator} as well as the life-cycle of shared component objects.
 */
@ActivityScope
public class BottomTabGridCoordinator implements Destroyable {
    private final BottomTabGridMediator mMediator;
    private final TabListCoordinator mTabGridCoordinator;
    private BottomTabGridSheetContent mBottomSheetContent;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;

    @Inject
    BottomTabGridCoordinator(@Named(ACTIVITY_CONTEXT) Context context,
            BottomSheetController bottomSheetController, TabModelSelector tabModelSelector,
            TabContentManager tabContentManager, ActivityLifecycleDispatcher lifecycleDispatcher) {
        mTabGridCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.GRID, context,
                tabModelSelector, tabContentManager, bottomSheetController.getBottomSheet());

        mMediator = new BottomTabGridMediator(bottomSheetController, this::resetWithTabModel);

        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
    }

    /**
     * Destroy any members that needs clean up.
     */
    @Override
    public void destroy() {
        mTabGridCoordinator.destroy();
        mLifecycleDispatcher.unregister(this);
        if (mBottomSheetContent != null) {
            mBottomSheetContent.destroy();
        }
    }

    /**
     * Updates tabs list through {@link TabListCoordinator} with given tab model
     * and calls onReset() on {@link BottomTabGridMediator}
     */
    void resetWithTabModel(TabModel tabModel) {
        mTabGridCoordinator.resetWithTabModel(tabModel);
        updateBottomSheetContent(tabModel);
        mMediator.onReset(mBottomSheetContent);
    }

    private void updateBottomSheetContent(TabModel tabModel) {
        if (tabModel != null) {
            // create and display sheet content
            mBottomSheetContent =
                    new BottomTabGridSheetContent(mTabGridCoordinator.getContainerView());
        } else {
            if (mBottomSheetContent != null) {
                mBottomSheetContent.destroy();
                mBottomSheetContent = null;
            }
        }
    }
}
