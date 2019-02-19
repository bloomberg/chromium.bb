// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.support.annotation.NonNull;

/**
 * An interface that provides and notifies about night mode state.
 */
public interface NightModeStateProvider {
    /** Observes night mode state changes. */
    interface Observer {
        /** Notifies on night mode state changed. */
        void onNightModeStateChanged();
    }

    /** @return Whether or not night mode is enabled. */
    boolean isInNightMode();

    /** @param observer The {@link Observer} to be registered to this provider. */
    void addObserver(@NonNull Observer observer);

    /** @param observer The {@link Observer} to be unregistered to this provider. */
    void removeObserver(@NonNull Observer observer);
}
