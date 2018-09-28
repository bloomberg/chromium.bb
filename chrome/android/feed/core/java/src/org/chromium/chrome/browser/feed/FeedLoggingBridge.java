// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.base.annotations.JNINamespace;

/**
 * Provides access to native implementation of feed logging.
 */
@JNINamespace("feed")
public class FeedLoggingBridge {
    private long mNativeFeedLoggingBridge;

    /**
     * Creates a {@link FeedLoggingBridge} for accessing native feed logging
     * implementation for the current user, and initial native side bridge.
     */
    public FeedLoggingBridge() {
        mNativeFeedLoggingBridge = nativeInit();
    }

    /** Cleans up native half of this bridge. */
    public void destroy() {
        assert mNativeFeedLoggingBridge != 0;
        nativeDestroy(mNativeFeedLoggingBridge);
        mNativeFeedLoggingBridge = 0;
    }

    public void onOpenedWithContent(int contentCount) {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnOpenedWithContent(mNativeFeedLoggingBridge, contentCount);
    }

    private native long nativeInit();
    private native void nativeDestroy(long nativeFeedLoggingBridge);
    private native void nativeOnOpenedWithContent(long nativeFeedLoggingBridge, int contentCount);
}
