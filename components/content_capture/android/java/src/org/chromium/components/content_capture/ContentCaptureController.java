// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * The abstract class to provide the whitelist and the runtime control of if ContentCapture should
 * start.
 */
@JNINamespace("content_capture")
public abstract class ContentCaptureController {
    /**
     * The singleton instance of ContentCaptureController, shall be set by subclass.
     */
    protected static ContentCaptureController sContentCaptureController;

    private long mNativeContentCaptureController;

    public static ContentCaptureController getInstance() {
        return sContentCaptureController;
    }

    protected ContentCaptureController() {
        mNativeContentCaptureController = nativeInit(this);
    }

    /**
     * @return if ContentCapture should be started for this app at all.
     */
    public abstract boolean shouldStartCapture();

    /**
     * Clear all ContentCapture data associated with Chrome.
     */
    public void clearAllContentCaptureData() {}

    /**
     * Clear ContentCapture data for specific URLs.
     */
    public void clearContentCaptureDataForURLs(String[] urlsToDelete) {}

    /**
     * Invoked by native side to pull the whitelist, the subclass should implement this and set
     * the whitelist by call setWhiteList.
     */
    @CalledByNative
    protected abstract void pullWhitelist();

    /**
     * Invoked by subclass to set the whitelist to native side. No whitelist (whitelist == null)
     * indicates everything is whitelisted, empty whitelist (whitelist.length == 0) indicates
     * nothing is whitelisted.
     *
     * @param whitelist the array of whitelist, it could be the hostname or the regex.
     * @param isRegex to indicate that the corresponding whitelist is the regex or not.
     */
    protected void setWhitelist(String[] whitelist, boolean[] isRegex) {
        nativeSetWhitelist(mNativeContentCaptureController, whitelist, isRegex);
    }

    private static native long nativeInit(Object contentCaptureController);
    private native void nativeSetWhitelist(
            long nativeContentCaptureController, String[] whitelist, boolean[] isRegex);
}
