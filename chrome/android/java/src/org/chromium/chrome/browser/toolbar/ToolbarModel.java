// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.content_public.browser.WebContents;

/**
 * Provides a way of accessing toolbar data and state.
 */
public class ToolbarModel {

    /**
     * Delegate for providing additional information to the model.
     */
    public interface ToolbarModelDelegate {
        /**
         * @return The currently active WebContents being used by the Toolbar.
         */
        @CalledByNative("ToolbarModelDelegate")
        WebContents getActiveWebContents();
    }

    private long mNativeToolbarModelAndroid;

    /**
     * Initialize the native counterpart of this model.
     * @param delegate The delegate that will be used by the model.
     */
    public void initialize(ToolbarModelDelegate delegate) {
        mNativeToolbarModelAndroid = nativeInit(delegate);
    }

    /**
     * Destroys the native ToolbarModel.
     */
    public void destroy() {
        if (mNativeToolbarModelAndroid == 0) return;
        nativeDestroy(mNativeToolbarModelAndroid);
        mNativeToolbarModelAndroid = 0;
    }

    /** @return The formatted URL suitable for editing. */
    public String getFormattedFullUrl() {
        if (mNativeToolbarModelAndroid == 0) return null;
        return nativeGetFormattedFullURL(mNativeToolbarModelAndroid);
    }

    /** @return The formatted URL suitable for display only. */
    public String getUrlForDisplay() {
        if (mNativeToolbarModelAndroid == 0) return null;
        return nativeGetURLForDisplay(mNativeToolbarModelAndroid);
    }

    private native long nativeInit(ToolbarModelDelegate delegate);
    private native void nativeDestroy(long nativeToolbarModelAndroid);
    private native String nativeGetFormattedFullURL(long nativeToolbarModelAndroid);
    private native String nativeGetURLForDisplay(long nativeToolbarModelAndroid);
}
