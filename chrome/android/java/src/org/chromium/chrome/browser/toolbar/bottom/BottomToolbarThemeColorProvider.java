// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.v7.content.res.AppCompatResources;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.chrome.browser.toolbar.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ThemeColorProvider.ThemeColorObserver;

/** A ThemeColorProvider for the bottom toolbar. */
public class BottomToolbarThemeColorProvider implements ThemeColorProvider, IncognitoStateObserver {
    /** List of {@link ThemeColorObserver}s. These are used to broadcast events to listeners. */
    private final ObserverList<ThemeColorObserver> mThemeColorObservers;

    /** Tint to be used in dark mode. */
    private final ColorStateList mDarkModeTint;

    /** Tint to be used in light mode. */
    private final ColorStateList mLightModeTint;

    /** Primary color for light mode. */
    private final int mLightPrimaryColor;

    /** Primary color for dark mode. */
    private final int mDarkPrimaryColor;

    /** Used to know when incognito mode is entered or exited. */
    private IncognitoStateProvider mIncognitoStateProvider;

    /** The overview mode manager. */
    private OverviewModeBehavior mOverviewModeBehavior;

    /** Observer to know when overview mode is entered/exited. */
    private OverviewModeObserver mOverviewModeObserver;

    /** Whether theme is dark mode. */
    private boolean mIsUsingDarkBackground;

    /** Whether app is in incognito mode. */
    private boolean mIsIncognito;

    /** Whether app is in overview mode. */
    private boolean mIsOverviewVisible;

    public BottomToolbarThemeColorProvider() {
        mThemeColorObservers = new ObserverList<ThemeColorObserver>();

        final Context context = ContextUtils.getApplicationContext();
        mDarkModeTint = AppCompatResources.getColorStateList(context, R.color.light_mode_tint);
        mLightModeTint = AppCompatResources.getColorStateList(context, R.color.dark_mode_tint);
        mLightPrimaryColor = ApiCompatibilityUtils.getColor(
                context.getResources(), R.color.modern_primary_color);
        mDarkPrimaryColor = ApiCompatibilityUtils.getColor(
                context.getResources(), R.color.incognito_modern_primary_color);

        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeStartedShowing(boolean showToolbar) {
                mIsOverviewVisible = true;
                updateTheme();
            }

            @Override
            public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
                mIsOverviewVisible = false;
                updateTheme();
            }
        };
    }

    @Override
    public void addObserver(ThemeColorObserver observer) {
        mThemeColorObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(ThemeColorObserver observer) {
        mThemeColorObservers.removeObserver(observer);
    }

    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mIncognitoStateProvider = provider;
        mIncognitoStateProvider.addIncognitoStateObserverAndTrigger(this);
    }

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        mIsIncognito = isIncognito;
        updateTheme();
    }

    void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        mOverviewModeBehavior = overviewModeBehavior;
        mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
    }

    private void updateTheme() {
        final boolean isAccessibilityEnabled = DeviceClassManager.enableAccessibilityLayout();
        final boolean isHorizontalTabSwitcherEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID);
        final boolean shouldUseDarkBackground = mIsIncognito
                && (isAccessibilityEnabled || isHorizontalTabSwitcherEnabled
                        || !mIsOverviewVisible);

        if (shouldUseDarkBackground == mIsUsingDarkBackground) return;
        mIsUsingDarkBackground = shouldUseDarkBackground;
        final int primaryColor = mIsUsingDarkBackground ? mDarkPrimaryColor : mLightPrimaryColor;
        final ColorStateList tint = mIsUsingDarkBackground ? mDarkModeTint : mLightModeTint;
        for (ThemeColorObserver observer : mThemeColorObservers) {
            observer.onThemeColorChanged(tint, primaryColor);
        }
    }

    void destroy() {
        if (mIncognitoStateProvider != null) {
            mIncognitoStateProvider.removeObserver(this);
            mIncognitoStateProvider = null;
        }
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
            mOverviewModeBehavior = null;
        }
        mThemeColorObservers.clear();
    }
}
