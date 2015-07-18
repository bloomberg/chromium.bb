// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offline_pages;

import org.chromium.base.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Access gate to C++ side offline pages functionalities.
 */
@JNINamespace("offline_pages::android")
public final class OfflinePageBridge {

    private long mNativeOfflinePageBridge;

    /**
     * Creates offline pages bridge for a given profile.
     */
    public OfflinePageBridge(Profile profile) {
        mNativeOfflinePageBridge = nativeInit(profile);
    }

    /**
     * Destroys native offline pages bridge. It should be called during
     * destruction to ensure proper cleanup.
     */
    public void destroy() {
        assert mNativeOfflinePageBridge != 0;
        nativeDestroy(mNativeOfflinePageBridge);
        mNativeOfflinePageBridge = 0;
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeOfflinePageBridge);
}
