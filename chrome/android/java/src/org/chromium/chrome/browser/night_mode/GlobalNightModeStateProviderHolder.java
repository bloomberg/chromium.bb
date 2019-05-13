// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.support.annotation.NonNull;
import android.support.v7.app.AppCompatDelegate;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.util.FeatureUtilities;

/**
 * Holds an instance of {@link NightModeStateProvider} that provides night mode state for the entire
 * application.
 */
public class GlobalNightModeStateProviderHolder {
    private static NightModeStateProvider sInstance;

    /**
     * Created when night mode is not available or not supported.
     */
    private static class DummyNightModeStateProvider implements NightModeStateProvider {
        private DummyNightModeStateProvider() {
            // Always stay in light mode if night mode is not available.
            AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_NO);
        }

        @Override
        public boolean isInNightMode() {
            return false;
        }

        @Override
        public void addObserver(@NonNull Observer observer) {}

        @Override
        public void removeObserver(@NonNull Observer observer) {}
    }

    /**
     * @return The {@link NightModeStateProvider} that maintains the night mode state for the entire
     *         application. Note that UI widgets should always get the
     *         {@link NightModeStateProvider} from the {@link ChromeBaseAppCompatActivity} they are
     *         attached to, because the night mode state can be overridden at the activity level.
     */
    public static NightModeStateProvider getInstance() {
        if (sInstance == null) {
            if (!NightModeUtils.isNightModeSupported()
                    || !FeatureUtilities.isNightModeAvailable()) {
                sInstance = new DummyNightModeStateProvider();
            } else {
                sInstance = new GlobalNightModeStateController(SystemNightModeMonitor.getInstance(),
                        ChromePreferenceManager.getInstance());
            }
        }
        return sInstance;
    }

    @VisibleForTesting
    static void resetInstanceForTesting() {
        sInstance = null;
    }
}
