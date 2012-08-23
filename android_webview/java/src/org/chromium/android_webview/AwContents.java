// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.view.ViewGroup;

import org.chromium.base.JNINamespace;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.common.CleanupReference;

/**
 * Exposes the native AwContents class, and together these classes wrap the ContentViewCore
 * and Browser components that are required to implement Android WebView API. This is the
 * primary entry point for the WebViewProvider implementation; it holds a 1:1 object
 * relationship with application WebView instances.
 * (We define this class independent of the hidden WebViewProvider interfaces, to allow
 * continuous build & test in the open source SDK-based tree).
 */
@JNINamespace("android_webview")
public class AwContents {

    private int mNativeAwContents;
    private ContentViewCore mContentViewCore;

    private static final class DestroyRunnable implements Runnable {
        private int mNativeAwContents;
        private DestroyRunnable(int nativeAwContents) {
            mNativeAwContents = nativeAwContents;
        }
        @Override
        public void run() {
            nativeDestroy(mNativeAwContents);
        }
    }

    private CleanupReference mCleanupReference;

    private AwContents(ContentViewCore contentViewCore,
            AwWebContentsDelegate webContentsDelegate) {
    }

    /**
     * @param containerView the view-hierarchy item this object will be bound to.
     * @param internalAccessAdapter to access private methods on containerView.
     * @param contentViewCore requires an existing but not yet initialized instance. Will be
     *                         initialized on return.
     * @param webContentsDelegate will receive API callbacks from the underlying WebContents
     * @param privateBrowsing whether this is a private browsing instance of WebView.
     * @param isAccessFromFileURLsGrantedByDefault passed to ContentViewCore.initialize.
     */
    public AwContents(ViewGroup containerView,
        ContentViewCore.InternalAccessDelegate internalAccessAdapter,
        ContentViewCore contentViewCore, AwWebContentsDelegate webContentsDelegate,
        boolean privateBrowsing, boolean isAccessFromFileURLsGrantedByDefault) {
      mNativeAwContents = nativeInit(webContentsDelegate, privateBrowsing);
      mContentViewCore = contentViewCore;
      mCleanupReference = new CleanupReference(this, new DestroyRunnable(mNativeAwContents));

      // TODO: upstream the needed ContentViewCore initialization method.
      // mContentViewCore.initialize(containerView, internalAccessAdapter, false,
      //     nativeGetWebContents(mNativeAwContents), isAccessFromFileURLsGrantedByDefault);
    }

    public ContentViewCore getContentViewCore() {
        return mContentViewCore;
    }

    public void destroy() {
        if (mContentViewCore != null) {
            mContentViewCore.destroy();
            mContentViewCore = null;
        }
        mCleanupReference.cleanupNow();
    }

    private native int nativeInit(AwWebContentsDelegate webViewWebContentsDelegate,
            boolean privateBrowsing);
    private static native void nativeDestroy(int nativeAwContents);

    private native int nativeGetWebContents(int nativeAwContents);
}
