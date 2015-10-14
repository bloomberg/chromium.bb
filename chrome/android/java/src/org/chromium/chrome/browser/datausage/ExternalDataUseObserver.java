// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.datausage;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.ChromeApplication;

/**
 * This class provides a base class implementation of a data use observer that is external to
 * Chromium. This class should be accessed only on IO thread.
 */
@JNINamespace("chrome::android")
public class ExternalDataUseObserver {
    /**
     * Pointer to the native ExternalDataUseObserver object.
     */
    protected long mNativeExternalDataUseObserver;

    @CalledByNative
    private static ExternalDataUseObserver create(Context context, long nativePtr) {
        return ((ChromeApplication) context).createExternalDataUseObserver(nativePtr);
    }

    /**
     * Notification that the native object has been destroyed.
     */
    @CalledByNative
    private void onDestroy() {
        mNativeExternalDataUseObserver = 0;
    }

    @CalledByNative
    protected void onDataUse(String tag, long bytesDownloaded, long bytesUploaded) {}

    /**
     * Creates an instance of {@link #ExternalDataUseObserver}.
     */
    public ExternalDataUseObserver(long nativePtr) {
        mNativeExternalDataUseObserver = nativePtr;
        assert mNativeExternalDataUseObserver != 0;
    }
}
