// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.chrome.browser.TabBase;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.LoadUrlParams;
import org.chromium.content.common.CleanupReference;
import org.chromium.ui.WindowAndroid;

/**
 * TestShell's implementation of a tab. This mirrors how Chrome for Android subclasses
 * and extends {@link TabBase}.
 */
public class TestShellTab extends TabBase {
    private int mNativeTestShellTab;

    private CleanupReference mCleanupReference;

    // Tab state
    private boolean mIsLoading = false;

    /**
     * @param context  The Context the view is running in.
     * @param url      The URL to start this tab with.
     * @param window   The WindowAndroid should represent this tab.
     */
    public TestShellTab(Context context, String url, WindowAndroid window) {
        super(false, context, window);
        initialize();
        initContentView();
        loadUrlWithSanitization(url);
    }

    @Override
    public void initialize() {
        super.initialize();

        mNativeTestShellTab = nativeInit();
        mCleanupReference = new CleanupReference(this, new DestroyRunnable(mNativeTestShellTab));
    }

    @Override
    public void destroy() {
        super.destroy();

        if (mNativeTestShellTab != 0) {
            mCleanupReference.cleanupNow();
            mNativeTestShellTab = 0;
        }
    }

    /**
     * @return Whether or not the tab is currently loading.
     */
    public boolean isLoading() {
        return mIsLoading;
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

        ContentView contentView = getContentView();
        if (TextUtils.equals(url, contentView.getUrl())) {
            contentView.reload();
        } else {
            contentView.loadUrl(new LoadUrlParams(url));
        }
    }

    @Override
    protected TabBaseChromeWebContentsDelegateAndroid createWebContentsDelegate() {
        return new TestShellTabBaseChromeWebContentsDelegateAndroid();
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

    private class TestShellTabBaseChromeWebContentsDelegateAndroid
            extends TabBaseChromeWebContentsDelegateAndroid {
        @Override
        public void onLoadStarted() {
            mIsLoading = true;
        }

        @Override
        public void onLoadStopped() {
            mIsLoading = false;
        }
    }

    private native int nativeInit();
    private static native void nativeDestroy(int nativeTestShellTab);
    private native String nativeFixupUrl(int nativeTestShellTab, String url);
}
