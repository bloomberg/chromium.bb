// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
 * This class receives callbacks that act as hooks for various a native web contents events related
 * to loading a url. A single web contents can have multiple WebContentObserverAndroids.
 */
@JNINamespace("content")
public abstract class WebContentsObserverAndroid {
    private int mNativeWebContentsObserverAndroid;

    public WebContentsObserverAndroid(ContentViewCore contentViewCore) {
        mNativeWebContentsObserverAndroid = nativeInit(contentViewCore.getNativeContentViewCore());
    }

    /**
     * Called when the a page starts loading.
     * @param url The validated url for the loading page.
     */
    @CalledByNative
    public void didStartLoading(String url) {
    }

    /**
     * Called when the a page finishes loading.
     * @param url The validated url for the page.
     */
    @CalledByNative
    public void didStopLoading(String url) {
    }

    /**
     * Called when an error occurs while loading a page and/or the page fails to load.
     * @param errorCode Error code for the occurring error.
     * @param description The description for the error.
     * @param failingUrl The url that was loading when the error occurred.
     */
    @CalledByNative
    public void didFailLoad(boolean isProvisionalLoad,
            boolean isMainFrame, int errorCode, String description, String failingUrl) {
    }

    /**
     * Called when the main frame of the page has committed.
     * @param url The validated url for the page.
     * @param baseUrl The validated base url for the page.
     */
    @CalledByNative
    public void didNavigateMainFrame(String url, String baseUrl) {
    }

    /**
     * Similar to didNavigateMainFrame but also called on subframe navigations.
     * @param url The validated url for the page.
     * @param baseUrl The validated base url for the page.
     * @param isReload True if this navigation is a reload.
     */
    @CalledByNative
    public void didNavigateAnyFrame(String url, String baseUrl, boolean isReload) {
    }

    /**
     * Destroy the corresponding native object.
     */
    @CalledByNative
    public void detachFromWebContents() {
        if (mNativeWebContentsObserverAndroid != 0) {
            nativeDestroy(mNativeWebContentsObserverAndroid);
            mNativeWebContentsObserverAndroid = 0;
        }
    }

    private native int nativeInit(int contentViewCorePtr);
    private native void nativeDestroy(int nativeWebContentsObserverAndroid);
}
