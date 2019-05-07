// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.support.annotation.Nullable;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.ui.system.SystemUiCoordinator;
import org.chromium.chrome.browser.widget.emptybackground.EmptyBackgroundViewWrapper;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * The root UI coordinator. This class will eventually be responsible for inflating and managing
 * lifecycle of the main UI components.
 *
 * The specific things this component will manage and how it will hook into Chrome*Activity are
 * still being discussed See https://crbug.com/931496.
 */
public class RootUiCoordinator implements Destroyable, NativeInitObserver, InflationObserver,
                                          ChromeActivity.MenuOrKeyboardActionHandler {
    private ChromeActivity mActivity;
    private AppMenuCoordinator mAppMenuCoordinator;
    private @Nullable ImmersiveModeManager mImmersiveModeManager;
    private SystemUiCoordinator mSystemUiCoordinator;

    /**
     * Create a new {@link RootUiCoordinator} for the given activity.
     * @param activity The containing {@link ChromeActivity}. TODO(https://crbug.com/931496):
     *         Remove this in favor of passing in direct dependencies.
     */
    public RootUiCoordinator(ChromeActivity activity) {
        mActivity = activity;
        mActivity.getLifecycleDispatcher().register(this);
        mActivity.registerMenuOrKeyboardActionHandler(this);
    }

    @Override
    public void destroy() {
        mActivity.unregisterMenuOrKeyboardActionHandler(this);
        mActivity = null;
        if (mSystemUiCoordinator != null) mSystemUiCoordinator.destroy();
        if (mImmersiveModeManager != null) mImmersiveModeManager.destroy();
        if (mAppMenuCoordinator != null) mAppMenuCoordinator.destroy();
    }

    @Override
    public void onPreInflationStartup() {}

    @Override
    public void onPostInflationStartup() {
        mAppMenuCoordinator = new AppMenuCoordinator(mActivity, mActivity.getLifecycleDispatcher(),
                mActivity.getToolbarManager(), mActivity, mActivity.getWindow().getDecorView());
        if (mActivity.getToolbarManager() != null) {
            mActivity.getToolbarManager().onAppMenuInitialized(
                    mAppMenuCoordinator.getAppMenuHandler(),
                    mAppMenuCoordinator.getAppMenuPropertiesDelegate());
        }

        mImmersiveModeManager = AppHooks.get().createImmersiveModeManager(
                mActivity.getWindow().getDecorView().findViewById(android.R.id.content));
        mSystemUiCoordinator =
                new SystemUiCoordinator(mActivity.getWindow(), mActivity.getTabModelSelector(),
                        mImmersiveModeManager, mActivity.getActivityType());

        if (mImmersiveModeManager != null && mActivity.getToolbarManager() != null) {
            mActivity.getToolbarManager().setImmersiveModeManager(mImmersiveModeManager);
        }
    }

    @Override
    public void onFinishNativeInitialization() {
        mSystemUiCoordinator.onNativeInitialized(mActivity.getOverviewModeBehavior());
        mAppMenuCoordinator.onNativeInitialized(mActivity.getOverviewModeBehavior());

        // TODO(twellington): Move to a TabbedRootUiCoordinator or delegate?
        if (mActivity.getActivityType() == ChromeActivity.ActivityType.TABBED
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            EmptyBackgroundViewWrapper bgViewWrapper = new EmptyBackgroundViewWrapper(
                    mActivity.getTabModelSelector(), mActivity.getTabCreator(false), mActivity,
                    mAppMenuCoordinator.getAppMenuHandler(), mActivity.getSnackbarManager(),
                    mActivity.getOverviewModeBehavior());
            bgViewWrapper.initialize();
        }
    }

    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (id == R.id.show_menu && mAppMenuCoordinator != null) {
            mAppMenuCoordinator.showAppMenuForKeyboardEvent();
            return true;
        }

        return false;
    }

    @VisibleForTesting
    public AppMenuCoordinator getAppMenuCoordinatorForTesting() {
        return mAppMenuCoordinator;
    }
}
