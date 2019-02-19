// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.support.annotation.NonNull;
import android.support.v7.app.AppCompatDelegate;

import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.util.FeatureUtilities;

/**
 * Maintains and provides the night mode state for {@link CustomTabActivity}.
 */
class CustomTabNightModeStateController implements NightModeStateProvider {
    CustomTabNightModeStateController(AppCompatDelegate delegate) {
        if (!FeatureUtilities.isNightModeAvailable()) return;
        delegate.setLocalNightMode(AppCompatDelegate.MODE_NIGHT_NO);
    }

    // NightModeStateProvider implementation.
    @Override
    public boolean isInNightMode() {
        // TODO(peconn): Implement this.
        return false;
    }

    @Override
    public void addObserver(@NonNull Observer observer) {
        // TODO(peconn): Implement this.
    }

    @Override
    public void removeObserver(@NonNull Observer observer) {
        // TODO(peconn): Implement this.
    }
}
