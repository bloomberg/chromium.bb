// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.support.annotation.NonNull;
import android.support.customtabs.CustomTabsIntent;
import android.support.v7.app.AppCompatDelegate;

import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.IntentUtils;

/**
 * Maintains and provides the night mode state for {@link CustomTabActivity}.
 */
class CustomTabNightModeStateController implements NightModeStateProvider {
    /**
     * The color scheme requested for the CCT. Only {@link CustomTabsIntent#COLOR_SCHEME_LIGHT}
     * and {@link CustomTabsIntent#COLOR_SCHEME_DARK} should be considered - fall back to the
     * system status for {@link CustomTabsIntent#COLOR_SCHEME_SYSTEM} when enabled.
     */
    private int mRequestedColorScheme;

    /**
     * Initializes the initial night mode state.
     * @param delegate The {@link AppCompatDelegate} that controls night mode state in support
     *                 library.
     * @param intent  The {@link Intent} to retrieve information about the initial state.
     */
    void initialize(AppCompatDelegate delegate, Intent intent) {
        if (!FeatureUtilities.isNightModeForCustomTabsAvailable()) {
            mRequestedColorScheme = CustomTabsIntent.COLOR_SCHEME_SYSTEM;
            return;
        }

        mRequestedColorScheme = IntentUtils.safeGetIntExtra(
                intent, CustomTabsIntent.EXTRA_COLOR_SCHEME, CustomTabsIntent.COLOR_SCHEME_SYSTEM);

        switch (mRequestedColorScheme) {
            case CustomTabsIntent.COLOR_SCHEME_LIGHT:
                delegate.setLocalNightMode(AppCompatDelegate.MODE_NIGHT_NO);
                break;
            case CustomTabsIntent.COLOR_SCHEME_DARK:
                delegate.setLocalNightMode(AppCompatDelegate.MODE_NIGHT_YES);
                break;
            default:
                break;
        }
    }

    // NightModeStateProvider implementation.
    @Override
    public boolean isInNightMode() {
        switch (mRequestedColorScheme) {
            case CustomTabsIntent.COLOR_SCHEME_LIGHT:
                return false;
            case CustomTabsIntent.COLOR_SCHEME_DARK:
                return true;
            default:
                // TODO(peter): Adhere to system status.
                return false;
        }
    }

    @Override
    public void addObserver(@NonNull Observer observer) {}

    @Override
    public void removeObserver(@NonNull Observer observer) {}

    @Override
    public boolean shouldOverrideConfiguration() {
        // Don't override configuration because the initial night mode state is only available
        // during CustomTabActivity#onCreate().
        return false;
    }
}
