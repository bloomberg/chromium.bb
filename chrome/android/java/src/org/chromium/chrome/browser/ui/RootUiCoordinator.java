// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.ui.system.SystemUiCoordinator;

/**
 * The root UI coordinator. This class will eventually be responsible for inflating and managing
 * lifecycle of the main UI components.
 *
 * The specific things this component will manage and how it will hook into Chrome*Activity are
 * still being discussed See https://crbug.com/931496.
 */
public class RootUiCoordinator implements Destroyable, NativeInitObserver {
    private SystemUiCoordinator mSystemUiCoordinator;
    private ChromeActivity mActivity;

    /**
     * Construct a new {@link RootUiCoordinator}.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the containing
     *         activity.
     * @param activity The containing {@link ChromeActivity}. TODO(https://crbug.com/931496):
     *         Remove this in favor of passing in direct dependencies.
     */
    public RootUiCoordinator(
            ActivityLifecycleDispatcher activityLifecycleDispatcher, ChromeActivity activity) {
        activityLifecycleDispatcher.register(this);
        mActivity = activity;
    }

    @Override
    public void destroy() {
        if (mSystemUiCoordinator != null) mSystemUiCoordinator.destroy();
    }

    @Override
    public void onFinishNativeInitialization() {
        mSystemUiCoordinator = new SystemUiCoordinator(mActivity.getWindow(),
                mActivity.getTabModelSelector(), mActivity.getOverviewModeBehavior());
    }
}
