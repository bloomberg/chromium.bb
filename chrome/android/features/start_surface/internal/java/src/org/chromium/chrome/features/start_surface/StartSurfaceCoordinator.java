// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tasks.TasksSurface;
import org.chromium.chrome.browser.tasks.TasksSurfaceProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate.TabSwitcherType;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarConfiguration;
import org.chromium.chrome.features.start_surface.StartSurfaceMediator.SurfaceMode;
import org.chromium.chrome.start_surface.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Root coordinator that is responsible for showing start surfaces, like a grid of Tabs, explore
 * surface and the bottom bar to switch between them.
 */
public class StartSurfaceCoordinator implements StartSurface {
    private final ChromeActivity mActivity;
    private final StartSurfaceMediator mStartSurfaceMediator;
    private final @SurfaceMode int mSurfaceMode;

    // Non-null in SurfaceMode.TASKS_ONLY, SurfaceMode.TWO_PANES and SurfaceMode.SINGLE_PANE modes.
    @Nullable
    private TasksSurface mTasksSurface;

    // Non-null in SurfaceMode.TASKS_ONLY, SurfaceMode.TWO_PANES and SurfaceMode.SINGLE_PANE modes.
    @Nullable
    private PropertyModelChangeProcessor mTasksSurfacePropertyModelChangeProcessor;

    // Non-null in SurfaceMode.SINGLE_PANE mode to show more tabs.
    @Nullable
    private TasksSurface mSecondaryTasksSurface;

    // Non-null in SurfaceMode.SINGLE_PANE mode to show more tabs.
    @Nullable
    private PropertyModelChangeProcessor mSecondaryTasksSurfacePropertyModelChangeProcessor;

    // Non-null in SurfaceMode.NO_START_SURFACE to show the tabs.
    @Nullable
    private TabSwitcher mTabSwitcher;

    // Non-null in SurfaceMode.TWO_PANES mode.
    @Nullable
    private BottomBarCoordinator mBottomBarCoordinator;

    // Non-null in SurfaceMode.TWO_PANES and SurfaceMode.SINGLE_PANE modes.
    @Nullable
    private ExploreSurfaceCoordinator mExploreSurfaceCoordinator;

    // Non-null in SurfaceMode.TWO_PANES and SurfaceMode.SINGLE_PANE modes.
    // TODO(crbug.com/982018): Get rid of this reference since the mediator keeps a reference to it.
    @Nullable
    private PropertyModel mPropertyModel;

    // Used to remember TabSwitcher.OnTabSelectingListener in SurfaceMode.SINGLE_PANE mode for more
    // tabs surface if necessary.
    @Nullable
    private TabSwitcher.OnTabSelectingListener mOnTabSelectingListener;

    // Whether the {@link initWithNative()} is called.
    private boolean mIsInitializedWithNative;

    // A flag of whether there is a pending call to {@link initialize()} but waiting for native's
    // initialization.
    private boolean mIsInitPending;

    private boolean mIsSecondaryTaskInitPending;

    public StartSurfaceCoordinator(ChromeActivity activity) {
        mActivity = activity;
        mSurfaceMode = computeSurfaceMode();

        boolean excludeMVTiles = StartSurfaceConfiguration.START_SURFACE_EXCLUDE_MV_TILES.getValue()
                || mSurfaceMode == SurfaceMode.OMNIBOX_ONLY
                || mSurfaceMode == SurfaceMode.NO_START_SURFACE;
        if (mSurfaceMode == SurfaceMode.NO_START_SURFACE) {
            // Create Tab switcher directly to save one layer in the view hierarchy.
            mTabSwitcher = TabManagementModuleProvider.getDelegate().createGridTabSwitcher(
                    mActivity, mActivity.getCompositorViewHolder());
        } else {
            createAndSetStartSurface(excludeMVTiles);
        }

        TabSwitcher.Controller controller =
                mTabSwitcher != null ? mTabSwitcher.getController() : mTasksSurface.getController();
        mStartSurfaceMediator = new StartSurfaceMediator(controller,
                mActivity.getTabModelSelector(), mPropertyModel,
                mSurfaceMode == SurfaceMode.SINGLE_PANE ? this::initializeSecondaryTasksSurface
                                                        : null,
                mSurfaceMode, mActivity.getNightModeStateProvider(),
                mActivity.getFullscreenManager(), this::isActivityFinishingOrDestroyed,
                excludeMVTiles,
                StartSurfaceConfiguration.START_SURFACE_SHOW_STACK_TAB_SWITCHER.getValue());
    }

    boolean isShowingTabSwitcher() {
        assert StartSurfaceConfiguration.isStartSurfaceStackTabSwitcherEnabled();
        return mStartSurfaceMediator.isShowingTabSwitcher();
    }

    // Implements StartSurface.
    @Override
    public void initialize() {
        // TODO (crbug.com/1041047): Move more stuff from the constructor to here for lazy
        // initialization.
        if (!mIsInitializedWithNative) {
            mIsInitPending = true;
            return;
        }

        mIsInitPending = false;
        if (mTasksSurface != null) {
            mTasksSurface.initialize();
        }
    }

    @Override
    public void setStateChangeObserver(StartSurface.StateObserver observer) {
        mStartSurfaceMediator.setStateChangeObserver(observer);
    }

    @Override
    public void setOnTabSelectingListener(StartSurface.OnTabSelectingListener listener) {
        if (mTasksSurface != null) {
            mTasksSurface.setOnTabSelectingListener(listener);
        } else {
            mTabSwitcher.setOnTabSelectingListener(listener);
        }

        // Set OnTabSelectingListener to the more tabs tasks surface as well if it has been
        // instantiated, otherwise remember it for the future instantiation.
        if (mSurfaceMode == SurfaceMode.SINGLE_PANE) {
            if (mSecondaryTasksSurface == null) {
                mOnTabSelectingListener = listener;
            } else {
                mSecondaryTasksSurface.setOnTabSelectingListener(listener);
            }
        }
    }

    @Override
    public void initWithNative() {
        if (mIsInitializedWithNative) return;

        mIsInitializedWithNative = true;
        if (mSurfaceMode == SurfaceMode.SINGLE_PANE || mSurfaceMode == SurfaceMode.TWO_PANES) {
            mExploreSurfaceCoordinator = new ExploreSurfaceCoordinator(mActivity,
                    mSurfaceMode == SurfaceMode.SINGLE_PANE ? mTasksSurface.getBodyViewContainer()
                                                            : mActivity.getCompositorViewHolder(),
                    mPropertyModel, mSurfaceMode == SurfaceMode.SINGLE_PANE);
        }
        mStartSurfaceMediator.initWithNative(mSurfaceMode != SurfaceMode.NO_START_SURFACE
                        ? mActivity.getToolbarManager().getFakeboxDelegate()
                        : null,
                mExploreSurfaceCoordinator != null
                        ? mExploreSurfaceCoordinator.getFeedSurfaceCreator()
                        : null);

        if (mTabSwitcher != null) {
            mTabSwitcher.initWithNative(mActivity, mActivity.getTabContentManager(),
                    mActivity.getCompositorViewHolder().getDynamicResourceLoader(), mActivity,
                    mActivity.getModalDialogManager());
        }
        if (mTasksSurface != null) {
            mTasksSurface.onFinishNativeInitialization(
                    mActivity, mActivity.getToolbarManager().getFakeboxDelegate());
        }

        if (mIsInitPending) {
            initialize();
        }

        if (mIsSecondaryTaskInitPending) {
            mIsSecondaryTaskInitPending = false;
            mSecondaryTasksSurface.onFinishNativeInitialization(
                    mActivity, mActivity.getToolbarManager().getFakeboxDelegate());
            mSecondaryTasksSurface.initialize();
        }
    }

    @Override
    public Controller getController() {
        return mStartSurfaceMediator;
    }

    @Override
    public TabSwitcher.TabListDelegate getTabListDelegate() {
        if (mTasksSurface != null) {
            return mTasksSurface.getTabListDelegate();
        }

        return mTabSwitcher.getTabListDelegate();
    }

    @Override
    public TabSwitcher.TabDialogDelegation getTabDialogDelegate() {
        return mTabSwitcher.getTabGridDialogDelegation();
    }

    @VisibleForTesting
    public boolean isInitPendingForTesting() {
        return mIsInitPending;
    }

    @VisibleForTesting
    public boolean isInitializedWithNativeForTesting() {
        return mIsInitializedWithNative;
    }

    @VisibleForTesting
    public boolean isSecondaryTaskInitPendingForTesting() {
        return mIsSecondaryTaskInitPending;
    }

    private @SurfaceMode int computeSurfaceMode() {
        // Check the cached flag before getting the parameter to be consistent with the other
        // places. Note that the cached flag may have been set before native initialization.
        if (!StartSurfaceConfiguration.isStartSurfaceEnabled()) {
            return SurfaceMode.NO_START_SURFACE;
        }

        String feature = StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue();

        if (feature.equals("twopanes")) {
            // Do not enable two panes when the bottom bar is enabled since it will
            // overlap the two panes' bottom bar.
            return BottomToolbarConfiguration.isBottomToolbarEnabled() ? SurfaceMode.SINGLE_PANE
                                                                       : SurfaceMode.TWO_PANES;
        }

        // TODO(crbug.com/982018): Remove isStartSurfaceSinglePaneEnabled check after
        // removing ChromePreferenceKeys.START_SURFACE_SINGLE_PANE_ENABLED_KEY.
        if (feature.equals("single")
                || StartSurfaceConfiguration.isStartSurfaceSinglePaneEnabled()) {
            return SurfaceMode.SINGLE_PANE;
        }

        if (feature.equals("tasksonly")) return SurfaceMode.TASKS_ONLY;

        if (feature.equals("omniboxonly")) return SurfaceMode.OMNIBOX_ONLY;

        // Default to SurfaceMode.TASKS_ONLY. This could happen when the start surface has been
        // changed from enabled to disabled in native side, but the cached flag has not been updated
        // yet, so StartSurfaceConfiguration.isStartSurfaceEnabled() above returns true.
        // TODO(crbug.com/1016548): Remember the last surface mode so as to default to it.
        return SurfaceMode.TASKS_ONLY;
    }

    private void createAndSetStartSurface(boolean excludeMVTiles) {
        ArrayList<PropertyKey> allProperties =
                new ArrayList<>(Arrays.asList(TasksSurfaceProperties.ALL_KEYS));
        allProperties.addAll(Arrays.asList(StartSurfaceProperties.ALL_KEYS));
        mPropertyModel = new PropertyModel(allProperties);

        int tabSwitcherType = mSurfaceMode == SurfaceMode.SINGLE_PANE ? TabSwitcherType.CAROUSEL
                                                                      : TabSwitcherType.GRID;
        if (StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY.getValue()) {
            tabSwitcherType = TabSwitcherType.SINGLE;
        }
        mTasksSurface = TabManagementModuleProvider.getDelegate().createTasksSurface(
                mActivity, mPropertyModel, tabSwitcherType, !excludeMVTiles);
        mTasksSurface.getView().setId(R.id.primary_tasks_surface_view);

        mTasksSurfacePropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mPropertyModel,
                        new TasksSurfaceViewBinder.ViewHolder(
                                mActivity.getCompositorViewHolder(), mTasksSurface.getView()),
                        TasksSurfaceViewBinder::bind);

        // There is nothing else to do for SurfaceMode.TASKS_ONLY and SurfaceMode.OMNIBOX_ONLY.
        if (mSurfaceMode == SurfaceMode.TASKS_ONLY || mSurfaceMode == SurfaceMode.OMNIBOX_ONLY) {
            return;
        }

        if (mSurfaceMode == SurfaceMode.TWO_PANES) {
            mBottomBarCoordinator = new BottomBarCoordinator(
                    mActivity, mActivity.getCompositorViewHolder(), mPropertyModel);
        }
    }

    private TabSwitcher.Controller initializeSecondaryTasksSurface() {
        assert mSurfaceMode == SurfaceMode.SINGLE_PANE;
        assert mSecondaryTasksSurface == null;

        PropertyModel propertyModel = new PropertyModel(TasksSurfaceProperties.ALL_KEYS);
        mStartSurfaceMediator.setSecondaryTasksSurfacePropertyModel(propertyModel);
        mSecondaryTasksSurface = TabManagementModuleProvider.getDelegate().createTasksSurface(
                mActivity, propertyModel,
                StartSurfaceConfiguration.isStartSurfaceStackTabSwitcherEnabled()
                        ? TabSwitcherType.NONE
                        : TabSwitcherType.GRID,
                false);
        if (mIsInitializedWithNative) {
            mSecondaryTasksSurface.onFinishNativeInitialization(
                    mActivity, mActivity.getToolbarManager().getFakeboxDelegate());
            mSecondaryTasksSurface.initialize();
        } else {
            mIsSecondaryTaskInitPending = true;
        }

        mSecondaryTasksSurface.getView().setId(R.id.secondary_tasks_surface_view);
        mSecondaryTasksSurfacePropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mPropertyModel,
                        new TasksSurfaceViewBinder.ViewHolder(mActivity.getCompositorViewHolder(),
                                mSecondaryTasksSurface.getView()),
                        SecondaryTasksSurfaceViewBinder::bind);
        if (mOnTabSelectingListener != null) {
            mSecondaryTasksSurface.setOnTabSelectingListener(mOnTabSelectingListener);
            mOnTabSelectingListener = null;
        }
        return mSecondaryTasksSurface.getController();
    }

    // TODO(crbug.com/1047488): This is a temporary solution of the issue crbug.com/1047488, which
    // has not been reproduced locally. The crash is because we can not find ChromeTabbedActivity's
    // ActivityInfo in the ApplicationStatus. However, from the code, ActivityInfo is created in
    // ApplicationStatus during AsyncInitializationActivity.onCreate, which happens before
    // ChromeTabbedActivity.startNativeInitialization where creates the Start surface. So one
    // possible reason is the ChromeTabbedActivity is finishing or destroyed when showing overview.
    private boolean isActivityFinishingOrDestroyed() {
        boolean finishingOrDestroyed = mActivity.isActivityFinishingOrDestroyed()
                || ApplicationStatus.getStateForActivity(mActivity) == ActivityState.DESTROYED;
        // TODO(crbug.com/1047488): Assert false. Do not do that in this CL to keep it small since
        // Start surface is eanbled in the fieldtrial_testing_config.json, which requires update of
        // the other browser tests.
        return finishingOrDestroyed;
    }
}
