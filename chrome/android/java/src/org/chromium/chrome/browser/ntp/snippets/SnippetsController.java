// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * The main controller for calling into the native snippets component to fetch snippets.
 */
public class SnippetsController {
    private static SnippetsController sInstance;

    private long mNativeSnippetsController;

    public SnippetsController() {}

    /**
     * Fetches new snippets.
     */
    public void fetchSnippets() {
        nativeFetchSnippets(Profile.getLastUsedProfile());
    }

    /**
     * Retrieve the singleton instance of this class.
     *
     * @return the singleton instance.
     */
    public static SnippetsController get() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new SnippetsController();
        }
        return sInstance;
    }

    @VisibleForTesting
    public static void setInstanceForTesting(SnippetsController instance) {
        sInstance = instance;
    }

    private native void nativeFetchSnippets(Profile profile);
}
