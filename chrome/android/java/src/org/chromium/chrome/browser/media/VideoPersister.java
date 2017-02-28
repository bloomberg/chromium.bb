// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivity;

/**
 * Utilities for persisting fullscreen video on Chrome exit.
 *
 * You should not instantiate this class yourself, only use it through {@link #getInstance()}.
 */
public class VideoPersister {
    private static VideoPersister sInstance;

    /**
     * Returns the singleton instance of VideoPersister, creating it if needed.
     */
    public static VideoPersister getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = AppHooks.get().createVideoPersister();
        }
        return sInstance;
    }

    public void attemptPersist(ChromeActivity activity) { }
    public void stopPersist(ChromeActivity activity) { }
}
