// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.ACTIVITY_CONTEXT;

import android.content.Context;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

import javax.inject.Inject;
import javax.inject.Named;

/**
 * A coordinator for BottomTabStrip component. Manages the communication with
 * {@link TabListCoordinator} & @{link BottomTabGridCoordinator} as well as the
 * life-cycle of shared component objects.
 */
@ActivityScope
public class TabStripBottomToolbarCoordinator
        implements Destroyable, TabStripBottomToolbarMediator.ResetHandler {
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final Context mContext;
    private final PropertyModel mTabStripToolbarModel;
    private BottomSheetController mBottomSheetController;
    private BottomTabGridCoordinator mBottomTabGridCoordinator;
    private TabContentManager mTabContentManager;
    private TabCreatorManager mTabCreatorManager;
    private TabListCoordinator mTabStripCoordinator;
    private TabModelSelector mTabModelSelector;
    private TabStripBottomToolbarMediator mMediator;
    private TabStripToolbarCoordinator mTabStripToolbarCoordinator;

    /**
     * Creates a new {@link TabStripBottomToolbarCoordinator}
     */
    @Inject
    public TabStripBottomToolbarCoordinator(@Named(ACTIVITY_CONTEXT) Context context,
            ChromeActivity activity, ActivityLifecycleDispatcher lifecycleDispatcher) {
        mContext = context;
        mLifecycleDispatcher = lifecycleDispatcher;
        mTabStripToolbarModel = new PropertyModel(TabStripToolbarViewProperties.ALL_KEYS);

        ViewStub stub = activity.findViewById(R.id.bottom_toolbar_stub);
        mTabStripToolbarCoordinator = new TabStripToolbarCoordinator(
                mContext, (ViewGroup) stub.inflate(), mTabStripToolbarModel);

        mLifecycleDispatcher.register(this);
    }

    /**
     * Handle any initialization that occurs once native has been loaded.
     */
    public void initializeWithNative(TabModelSelector tabModelSelector,
            TabContentManager tabContentManager, TabCreatorManager tabCreatorManager,
            BottomSheetController bottomSheetController) {
        mTabModelSelector = tabModelSelector;
        mTabContentManager = tabContentManager;
        mTabCreatorManager = tabCreatorManager;
        mBottomSheetController = bottomSheetController;

        mTabStripCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.STRIP,
                mContext, mTabModelSelector, mTabContentManager,
                mTabStripToolbarCoordinator.getTabListContainerView(), true);

        mBottomTabGridCoordinator = new BottomTabGridCoordinator(mContext, mBottomSheetController,
                mTabModelSelector, mTabContentManager, mTabCreatorManager);

        mMediator = new TabStripBottomToolbarMediator(
                this, mTabStripToolbarModel, mTabModelSelector, mTabCreatorManager);
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
        mMediator.resetWithTabModel(tabModel);
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
        mLifecycleDispatcher.unregister(this);
    }
}
