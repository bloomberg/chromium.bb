// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.support.annotation.Nullable;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.appmenu.AppMenuCoordinatorFactory;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.InflationObserver;

/**
 * The root UI coordinator. This class will eventually be responsible for inflating and managing
 * lifecycle of the main UI components.
 *
 * The specific things this component will manage and how it will hook into Chrome*Activity are
 * still being discussed See https://crbug.com/931496.
 */
public class RootUiCoordinator
        implements Destroyable, InflationObserver, ChromeActivity.MenuOrKeyboardActionHandler {
    protected ChromeActivity mActivity;
    protected @Nullable AppMenuCoordinator mAppMenuCoordinator;

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
        if (mAppMenuCoordinator != null) mAppMenuCoordinator.destroy();
    }

    @Override
    public void onPreInflationStartup() {}

    @Override
    public void onPostInflationStartup() {
        // TODO(https://crbug.com/931496): Revisit this as part of the broader
        // discussion around activity-specific UI customizations.
        if (mActivity.supportsAppMenu()) {
            mAppMenuCoordinator = AppMenuCoordinatorFactory.createAppMenuCoordinator(mActivity,
                    mActivity.getLifecycleDispatcher(), mActivity.getToolbarManager(), mActivity,
                    mActivity.getWindow().getDecorView(),
                    mActivity.getOverviewModeBehaviorSupplier());
            mActivity.getToolbarManager().onAppMenuInitialized(
                    mAppMenuCoordinator.getAppMenuHandler(),
                    mAppMenuCoordinator.getAppMenuPropertiesDelegate());
        } else if (mActivity.getToolbarManager() != null) {
            mActivity.getToolbarManager().getToolbar().disableMenuButton();
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
