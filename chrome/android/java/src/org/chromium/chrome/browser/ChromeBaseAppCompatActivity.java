// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;

import org.chromium.chrome.browser.night_mode.GlobalNightModeStateController;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;

/**
 * A subclass of {@link AppCompatActivity} that maintains states applied to all activities in
 * {@link ChromeApplication} (e.g. night mode).
 */
public class ChromeBaseAppCompatActivity
        extends AppCompatActivity implements NightModeStateProvider.Observer {
    private NightModeStateProvider mNightModeStateProvider;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        mNightModeStateProvider = createNightModeStateProvider();
        mNightModeStateProvider.addObserver(this);
        super.onCreate(savedInstanceState);
    }

    @Override
    protected void onDestroy() {
        mNightModeStateProvider.removeObserver(this);
        super.onDestroy();
    }

    /**
     * @return The {@link NightModeStateProvider} that provides the state of night mode.
     */
    public final NightModeStateProvider getNightModeStateProvider() {
        return mNightModeStateProvider;
    }

    /**
     * @return The {@link NightModeStateProvider} that provides the state of night mode in the scope
     *         of this class.
     */
    protected NightModeStateProvider createNightModeStateProvider() {
        return GlobalNightModeStateController.getInstance();
    }

    // NightModeStateProvider.Observer implementation.
    @Override
    public void onNightModeStateChanged() {
        recreate();
    }
}
