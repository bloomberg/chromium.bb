// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.app.Activity;
import android.content.res.Configuration;
import android.os.Build;
import android.support.annotation.StyleRes;

import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;

/**
 * Helper methods for supporting night mode.
 */
public class NightModeUtils {
    /**
     * Updates configuration for night mode to ensure night mode settings are applied properly.
     * Should be called anytime the Activity's configuration changes (e.g. from
     * {@link Activity#onConfigurationChanged(Configuration)}) if uiMode was not overridden on
     * the configuration during activity initialization
     * (see {@link #applyOverridesForNightMode(NightModeStateProvider, Configuration)}).
     * @param activity The {@link ChromeBaseAppCompatActivity} that needs to be updated.
     * @param newConfig The new {@link Configuration} from
     *                  {@link Activity#onConfigurationChanged(Configuration)}.
     * @param themeResId The {@link StyleRes} for the theme of the specified activity.
     */
    public static void updateConfigurationForNightMode(ChromeBaseAppCompatActivity activity,
            Configuration newConfig, @StyleRes int themeResId) {
        final int uiNightMode = activity.getNightModeStateProvider().isInNightMode()
                ? Configuration.UI_MODE_NIGHT_YES
                : Configuration.UI_MODE_NIGHT_NO;

        if (uiNightMode == (newConfig.uiMode & Configuration.UI_MODE_NIGHT_MASK)) return;

        // This is to fix styles on floating action bar when the new configuration UI mode doesn't
        // match the actual UI mode we need, and NightModeStateProvider#shouldOverrideConfiguration
        // returns false. May check if this is needed on newer version of support library.
        // See https://crbug.com/935731.
        if (themeResId != 0) {
            // Re-apply theme so that the correct configuration is used.
            activity.setTheme(themeResId);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                // On M+ setTheme only applies if the themeResId actually changes. Force applying
                // the styles so that the correct styles are used.
                activity.getTheme().applyStyle(themeResId, true);
            }
        }
    }

    /**
     * @param provider The {@link NightModeStateProvider} that provides the night mode state.
     * @param config The {@link Configuration} on which UI night mode should be overridden if
     *               necessary.
     * @return True if UI night mode is overridden on the provided {@code config}, and false
     *         otherwise.
     */
    public static boolean applyOverridesForNightMode(
            NightModeStateProvider provider, Configuration config) {
        if (!provider.shouldOverrideConfiguration()) return false;

        // Override uiMode so that UIs created by the DecorView (e.g. context menu, floating
        // action bar) get the correct theme. May check if this is needed on newer version
        // of support library. See https://crbug.com/935731.
        final int nightMode = provider.isInNightMode() ? Configuration.UI_MODE_NIGHT_YES
                                                       : Configuration.UI_MODE_NIGHT_NO;
        config.uiMode = nightMode | (config.uiMode & ~Configuration.UI_MODE_NIGHT_MASK);
        return true;
    }
}
