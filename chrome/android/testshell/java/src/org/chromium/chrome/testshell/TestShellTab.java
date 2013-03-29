// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.ChromeWebContentsDelegateAndroid;
import org.chromium.chrome.browser.ContentViewUtil;
import org.chromium.chrome.browser.TabBase;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.LoadUrlParams;
import org.chromium.content.common.CleanupReference;
import org.chromium.ui.gfx.NativeWindow;

/**
 * TestShell's implementation of a tab. This mirrors how Chrome for Android subclasses
 * and extends {@link TabBase}.
 */
public class TestShellTab extends TabBase {
    private ChromeWebContentsDelegateAndroid mWebContentsDelegate;
    private ContentView mContentView;
    private int mNativeTestShellTab;
    private final ObserverList<TestShellTabObserver> mObservers =
        new ObserverList<TestShellTabObserver>();

    private CleanupReference mCleanupReference;

    // Tab state
    private boolean mIsLoading = false;

    /**
     * @param context  The Context the view is running in.
     * @param url      The URL to start this tab with.
     * @param window   The NativeWindow should represent this tab.
     */
    public TestShellTab(Context context, String url, NativeWindow window) {
        super(window);
        init(context);
        loadUrlWithSanitization(url);
    }

    /**
     * @param context              The Context the view is running in.
     */
    private void init(Context context) {
        // Build the WebContents and the ContentView/ContentViewCore
        int nativeWebContentsPtr = ContentViewUtil.createNativeWebContents(false);
        mContentView = ContentView.newInstance(context, nativeWebContentsPtr, getNativeWindow(),
                ContentView.PERSONALITY_CHROME);
        mNativeTestShellTab = nativeInit(nativeWebContentsPtr,
                getNativeWindow().getNativePointer());

        // Build the WebContentsDelegate
        mWebContentsDelegate = new TabBaseChromeWebContentsDelegateAndroid();
        nativeInitWebContentsDelegate(mNativeTestShellTab, mWebContentsDelegate);

        // To be called after everything is initialized.
        mCleanupReference = new CleanupReference(this,
                new DestroyRunnable(mNativeTestShellTab));
    }

    /**
     * Should be called when the tab is no longer needed.  Once this is called this tab should not
     * be used.
     */
    public void destroy() {
        for (TestShellTabObserver observer : mObservers) {
            observer.onCloseTab(TestShellTab.this);
        }
        destroyContentView();
        if (mNativeTestShellTab != 0) {
            mCleanupReference.cleanupNow();
            mNativeTestShellTab = 0;
        }
    }

    /**
     * @param observer The {@link TestShellTabObserver} that should be notified of changes.
     */
    public void addObserver(TestShellTabObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * @param observer The {@link TestShellTabObserver} that should no longer be notified of
     * changes.
     */
    public void removeObserver(TestShellTabObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * @return Whether or not the tab is currently loading.
     */
    public boolean isLoading() {
        return mIsLoading;
    }

    /**
     * @return The {@link ContentView} represented by this tab.
     */
    public ContentView getContentView() {
        return mContentView;
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
        url = nativeFixupUrl(mNativeTestShellTab, url);

        // Invalid URLs will just return empty.
        if (TextUtils.isEmpty(url)) return;

        if (TextUtils.equals(url, mContentView.getUrl())) {
            mContentView.reload();
        } else {
            mContentView.loadUrl(new LoadUrlParams(url));
        }
    }

    private static final class DestroyRunnable implements Runnable {
        private final int mNativeTestShellTab;
        private DestroyRunnable(int nativeTestShellTab) {
            mNativeTestShellTab = nativeTestShellTab;
        }
        @Override
        public void run() {
            nativeDestroy(mNativeTestShellTab);
        }
    }

    private class TabBaseChromeWebContentsDelegateAndroid
            extends ChromeWebContentsDelegateAndroid {
        @Override
        public void onLoadProgressChanged(int progress) {
            for (TestShellTabObserver observer : mObservers) {
                observer.onLoadProgressChanged(TestShellTab.this, progress);
            }
        }

        @Override
        public void onUpdateUrl(String url) {
            for (TestShellTabObserver observer : mObservers) {
                observer.onUpdateUrl(TestShellTab.this, url);
            }
        }

        @Override
        public void onLoadStarted() {
            mIsLoading = true;
        }

        @Override
        public void onLoadStopped() {
            mIsLoading = false;
        }
    }

    private native int nativeInit(int webContentsPtr, int windowAndroidPtr);
    private static native void nativeDestroy(int nativeTestShellTab);
    private native void nativeInitWebContentsDelegate(int nativeTestShellTab,
            ChromeWebContentsDelegateAndroid chromeWebContentsDelegateAndroid);
    private native String nativeFixupUrl(int nativeTestShellTab, String url);
}
