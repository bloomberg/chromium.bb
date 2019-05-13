// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.system;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.res.Resources;
import android.graphics.Color;
import android.os.Build;
import android.support.annotation.Nullable;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.BuildInfo;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.ui.ImmersiveModeManager;
import org.chromium.chrome.browser.vr.VrModeObserver;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.ui.UiUtils;

/**
 * Controls the bottom system navigation bar color for the provided {@link Window}.
 */
@TargetApi(Build.VERSION_CODES.O_MR1)
class NavigationBarColorController implements VrModeObserver {
    private final Window mWindow;
    private final ViewGroup mRootView;
    private final Resources mResources;

    // May be null if we return from the constructor early. Otherwise will be set.
    private final @Nullable TabModelSelector mTabModelSelector;
    private final @Nullable TabModelSelectorObserver mTabModelSelectorObserver;
    private @Nullable OverviewModeBehavior mOverviewModeBehavior;
    private @Nullable OverviewModeObserver mOverviewModeObserver;

    private boolean mInitialized;
    private boolean mUseLightNavigation;
    private boolean mOverviewModeHiding;

    /**
     * Creates a new {@link NavigationBarColorController} instance.
     * @param window The {@link Window} this controller should operate on.
     * @param tabModelSelector The {@link TabModelSelector} used to determine which tab model is
     *                         selected.
     * @param immersiveModeManager The {@link ImmersiveModeManager} for the containing activity.
     */
    NavigationBarColorController(Window window, TabModelSelector tabModelSelector,
            @Nullable ImmersiveModeManager immersiveModeManager) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1;

        mWindow = window;
        mRootView = (ViewGroup) mWindow.getDecorView().getRootView();
        mResources = mRootView.getResources();

        if (immersiveModeManager != null && immersiveModeManager.isImmersiveModeSupported()) {
            // TODO(https://crbug.com/937946): Hook up immersive mode observer.
            mTabModelSelector = null;
            mTabModelSelectorObserver = null;

            window.setNavigationBarColor(Color.TRANSPARENT);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                window.setNavigationBarDividerColor(Color.TRANSPARENT);
            }
            int visibility =
                    mRootView.getSystemUiVisibility() & ~View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
            mRootView.setSystemUiVisibility(visibility);

            return;
        }

        // If we're not using a light navigation bar, it will always be black so there's no need
        // to register observers and manipulate coloring.
        if (!mResources.getBoolean(R.bool.window_light_navigation_bar) || BuildInfo.isAtLeastQ()) {
            mTabModelSelector = null;
            mTabModelSelectorObserver = null;
            return;
        }

        mInitialized = true;
        mUseLightNavigation = true;

        mTabModelSelector = tabModelSelector;
        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                updateNavigationBarColor();
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        // TODO(https://crbug.com/806054): Observe tab loads to restrict black bottom nav to
        // incognito NTP.

        updateNavigationBarColor();

        VrModuleProvider.registerVrModeObserver(this);
    }

    /**
     * Destroy this {@link NavigationBarColorController} instance.
     */
    public void destroy() {
        if (mTabModelSelector != null) mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
        }
        VrModuleProvider.unregisterVrModeObserver(this);
    }

    /**
     * @param overviewModeBehavior The {@link OverviewModeBehavior} used to determine whether
     *                             overview mode is showing.
     */
    public void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        if (!mInitialized) return;

        mOverviewModeBehavior = overviewModeBehavior;
        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeStartedShowing(boolean showToolbar) {
                mOverviewModeHiding = false;
                updateNavigationBarColor();
            }

            @Override
            public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
                mOverviewModeHiding = true;
                updateNavigationBarColor();
            }

            @Override
            public void onOverviewModeFinishedHiding() {
                mOverviewModeHiding = false;
            }
        };
        mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
        updateNavigationBarColor();
    }

    @Override
    public void onExitVr() {
        // The platform ignores the light navigation bar system UI flag when launching an Activity
        // in VR mode, so we need to restore it when VR is exited.
        // TODO(https://crbug.com/937946): How does this interact with immersive mode?
        UiUtils.setNavigationBarIconColor(mRootView, mUseLightNavigation);
    }

    @Override
    public void onEnterVr() {}

    @SuppressLint("NewApi")
    private void updateNavigationBarColor() {
        boolean overviewVisible = mOverviewModeBehavior != null
                && mOverviewModeBehavior.overviewVisible() && !mOverviewModeHiding;

        boolean useLightNavigation;
        if (ChromeFeatureList.isInitialized()
                && (ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID)
                        || DeviceClassManager.enableAccessibilityLayout())) {
            useLightNavigation = !mTabModelSelector.isIncognitoSelected();
        } else {
            useLightNavigation = !mTabModelSelector.isIncognitoSelected() || overviewVisible;
        }

        useLightNavigation &= !UiUtils.isSystemUiThemingDisabled();

        if (mUseLightNavigation == useLightNavigation) return;

        mUseLightNavigation = useLightNavigation;

        mWindow.setNavigationBarColor(useLightNavigation ? ApiCompatibilityUtils.getColor(
                                              mResources, R.color.bottom_system_nav_color)
                                                         : Color.BLACK);

        setNavigationBarColor(useLightNavigation);

        UiUtils.setNavigationBarIconColor(mRootView, useLightNavigation);
    }

    @SuppressLint("NewApi")
    private void setNavigationBarColor(boolean useLightNavigation) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mWindow.setNavigationBarDividerColor(useLightNavigation
                            ? ApiCompatibilityUtils.getColor(
                                    mResources, R.color.bottom_system_nav_divider_color)
                            : Color.BLACK);
        }
    }
}
