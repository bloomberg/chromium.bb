// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.chrome.browser.ChromeWebContentsDelegateAndroid;
import org.chromium.chrome.browser.ContentViewUtil;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.LoadUrlParams;
import org.chromium.content.common.CleanupReference;
import org.chromium.ui.gfx.NativeWindow;

/**
 * The basic Java representation of a tab.  Contains and manages a {@link ContentView}.
 */
public class TabBase {
    private NativeWindow mWindow;
    private ContentView mContentView;
    private ChromeWebContentsDelegateAndroid mWebContentsDelegate;
    private int mNativeTabBaseAndroidImpl;

    private CleanupReference mCleanupReference;

    /**
     * @param context  The Context the view is running in.
     * @param url      The URL to start this tab with.
     * @param window   The NativeWindow should represent this tab.
     * @param delegate The {@link ChromeWebContentsDelegateAndroid} that should be notified of any
     *                 WebContents changes.
     */
    public TabBase(Context context, String url, NativeWindow window,
            ChromeWebContentsDelegateAndroid delegate) {
        this(context, 0, window, delegate);
        loadUrlWithSanitization(url);
    }

    /**
     * @param context           The Context the view is running in.
     * @param nativeWebContents A native pointer to the WebContents this tab represents.
     * @param window            The NativeWindow should represent this tab.
     * @param delegate          The {@link ChromeWebContentsDelegateAndroid} that should be notified
     *                          of any WebContents changes.
     */
    public TabBase(Context context, int nativeWebContentsPtr,
            NativeWindow window, ChromeWebContentsDelegateAndroid delegate) {
        mWindow = window;

        // Build the WebContents and the ContentView/ContentViewCore
        if (nativeWebContentsPtr == 0) {
            nativeWebContentsPtr = ContentViewUtil.createNativeWebContents(false);
        }
        mContentView = ContentView.newInstance(context, nativeWebContentsPtr, mWindow,
                ContentView.PERSONALITY_CHROME);
        mNativeTabBaseAndroidImpl = nativeInit(nativeWebContentsPtr);

        // Build the WebContentsDelegate
        mWebContentsDelegate = delegate == null ? new ChromeWebContentsDelegateAndroid() : delegate;
        nativeInitWebContentsDelegate(mNativeTabBaseAndroidImpl, mWebContentsDelegate);

        // To be called after everything is initialized.
        mCleanupReference = new CleanupReference(this,
                new DestroyRunnable(mNativeTabBaseAndroidImpl));
    }

    /**
     * Should be called when the tab is no longer needed.  Once this is called this tab should not
     * be used.
     */
    public void destroy() {
        destroyContentView();
        if (mNativeTabBaseAndroidImpl != 0) {
            mCleanupReference.cleanupNow();
            mNativeTabBaseAndroidImpl = 0;
        }
    }

    /**
     * @return The {@link ContentView} represented by this tab.
     */
    public ContentView getContentView() {
        return mContentView;
    }

    /**
     * @return A pointer to the native component of this tab.
     */
    public int getNativeTab() {
        return mNativeTabBaseAndroidImpl;
    }

    private void destroyContentView() {
        if (mContentView == null) return;

        mContentView.destroy();
        mContentView = null;
    }

    /**
     * Navigates this Tab's {@link ContentView} to a sanitized version of {@code url}.
     * @param url The potentially unsanitized URL to navigate to.
     */
    public void loadUrlWithSanitization(String url) {
        if (url == null) return;

        // Sanitize the URL.
        url = nativeFixupUrl(mNativeTabBaseAndroidImpl, url);

        // Invalid URLs will just return empty.
        if (TextUtils.isEmpty(url)) return;

        if (TextUtils.equals(url, mContentView.getUrl())) {
            mContentView.reload();
        } else {
            mContentView.loadUrl(new LoadUrlParams(url));
        }
    }

    private static final class DestroyRunnable implements Runnable {
        private final int mNativeTabBaseAndroidImpl;
        private DestroyRunnable(int nativeTabBaseAndroidImpl) {
            mNativeTabBaseAndroidImpl = nativeTabBaseAndroidImpl;
        }
        @Override
        public void run() {
            nativeDestroy(mNativeTabBaseAndroidImpl);
        }
    }

    private native int nativeInit(int webContentsPtr);
    private static native void nativeDestroy(int nativeTabBaseAndroidImpl);
    private native void nativeInitWebContentsDelegate(int nativeTabBaseAndroidImpl,
            ChromeWebContentsDelegateAndroid chromeWebContentsDelegateAndroid);
    private native String nativeFixupUrl(int nativeTabBaseAndroidImpl, String url);
}