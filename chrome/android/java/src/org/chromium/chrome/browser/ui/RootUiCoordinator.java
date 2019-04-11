// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.support.annotation.Nullable;

import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.ui.system.SystemUiCoordinator;

/**
 * The root UI coordinator. This class will eventually be responsible for inflating and managing
 * lifecycle of the main UI components.
 *
 * The specific things this component will manage and how it will hook into Chrome*Activity are
 * still being discussed See https://crbug.com/931496.
 */
public class RootUiCoordinator implements Destroyable, NativeInitObserver, InflationObserver {
    private ChromeActivity mActivity;
    private @Nullable ImmersiveModeManager mImmersiveModeManager;
    private SystemUiCoordinator mSystemUiCoordinator;

    /**
     * Create a new {@link RootUiCoordinator} for the given activity.
     * @param activity The containing {@link ChromeActivity}. TODO(https://crbug.com/931496):
     *         Remove this in favor of passing in direct dependencies.
     */
    public static void create(ChromeActivity activity) {
        new RootUiCoordinator(activity);
    }

    RootUiCoordinator(ChromeActivity activity) {
        mActivity = activity;
        mActivity.getLifecycleDispatcher().register(this);
    }

    @Override
    public void destroy() {
        mActivity = null;
        if (mSystemUiCoordinator != null) mSystemUiCoordinator.destroy();
        if (mImmersiveModeManager != null) mImmersiveModeManager.destroy();
    }

    @Override
    public void onPreInflationStartup() {}

    @Override
    public void onPostInflationStartup() {
        mImmersiveModeManager = AppHooks.get().createImmersiveModeManager(
                mActivity.getWindow().getDecorView().findViewById(android.R.id.content));
        mSystemUiCoordinator =
                new SystemUiCoordinator(mActivity.getWindow(), mActivity.getTabModelSelector(),
                        mImmersiveModeManager, mActivity.getActivityType());

        if (mImmersiveModeManager != null) {
            mActivity.getToolbarManager().setImmersiveModeManager(mImmersiveModeManager);
        }
    }

    @Override
    public void onFinishNativeInitialization() {
        mSystemUiCoordinator.onNativeInitialized(mActivity.getOverviewModeBehavior());
    }
}
