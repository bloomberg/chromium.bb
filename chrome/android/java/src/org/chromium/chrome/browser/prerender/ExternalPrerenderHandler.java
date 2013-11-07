// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prerender;

import org.chromium.base.JNINamespace;
import org.chromium.chrome.browser.ContentViewUtil;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * A handler class for prerender requests coming from  other applications.
 */
@JNINamespace("prerender")
public class ExternalPrerenderHandler {

    private int mNativeExternalPrerenderHandler;

    public ExternalPrerenderHandler() {
        mNativeExternalPrerenderHandler = nativeInit();
    }

    public int addPrerender(Profile profile, String url, String referrer, int width, int height) {
        int webContentsPtr = ContentViewUtil.createNativeWebContents(false);
        if (nativeAddPrerender(mNativeExternalPrerenderHandler, profile, webContentsPtr,
                url, referrer, width, height)) {
            return webContentsPtr;
        }
        ContentViewUtil.destroyNativeWebContents(webContentsPtr);
        return 0;
    }

    public void cancelCurrentPrerender() {
        nativeCancelCurrentPrerender(mNativeExternalPrerenderHandler);
    }

    public static boolean hasPrerenderedUrl(Profile profile, String url, int webContentsPtr)  {
        return nativeHasPrerenderedUrl(profile, url, webContentsPtr);
    }

    private static native int nativeInit();
    private static native boolean nativeAddPrerender(
            int nativeExternalPrerenderHandlerAndroid, Profile profile,
            int webContentsPtr, String url, String referrer, int width, int height);
    private static native boolean nativeHasPrerenderedUrl(
            Profile profile, String url, int webContentsPtr);
    private static native void nativeCancelCurrentPrerender(
            int nativeExternalPrerenderHandlerAndroid);
}
