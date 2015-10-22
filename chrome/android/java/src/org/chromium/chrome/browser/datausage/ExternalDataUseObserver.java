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
     * Creates an instance of {@link #ExternalDataUseObserver}.
     * @param nativePtr pointer to the native ExternalDataUseObserver object.
     */
    public ExternalDataUseObserver(long nativePtr) {
        mNativeExternalDataUseObserver = nativePtr;
        assert mNativeExternalDataUseObserver != 0;
    }

    /**
     * Notification that the native object has been destroyed.
     */
    @CalledByNative
    private void onDestroy() {
        mNativeExternalDataUseObserver = 0;
    }

    /**
    * Fetches matching rules asynchronously. While an asynchronous fetch is underway, it is illegal
    * to make calls to this method.
    * @return true if {@link #nativeFetchMatchingRulesCallback} will eventually be called with
    * matching rules, and false if matching rules cannot be retrieved.
    */
    @CalledByNative
    protected boolean fetchMatchingRules() {
        return false;
    }

    /**
     * Submits a data use report asynchronously.
     * @param tag tag of the report.
     * @param bytesDownloaded number of bytes downloaded by Chromium.
     * @param bytesUploaded number of bytes uploaded by Chromium.
     * The result of this request is returned asynchronously via
     * {@link #nativeOnDataUseCallback}. A new report should be submitted only after the result
     * has been returned via {@link #nativeOnDataUseCallback}.
     */
    @CalledByNative
    protected void submitDataUseReport(String tag, long bytesDownloaded, long bytesUploaded) {}

    public native void nativeFetchMatchingRulesCallback(long nativeExternalDataUseObserver);

    public native void nativeSubmitDataUseReportCallback(long nativeExternalDataUseObserver);
}
