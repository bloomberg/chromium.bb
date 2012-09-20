// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.AsyncTask;
import android.os.Message;
import android.text.TextUtils;
import android.util.Log;
import android.view.ViewGroup;
import android.webkit.ValueCallback;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.NavigationHistory;
import org.chromium.content.common.CleanupReference;
import org.chromium.ui.gfx.NativeWindow;

import java.io.File;
import java.net.MalformedURLException;
import java.net.URL;

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
    private static final String TAG = AwContents.class.getSimpleName();

    private static final String WEB_ARCHIVE_EXTENSION = ".mht";

    private int mNativeAwContents;
    private ContentViewCore mContentViewCore;
    private AwContentsClient mContentsClient;
    private AwContentsIoThreadClient mIoThreadClient;

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
     * @param contentsClient will receive API callbacks from this WebView Contents
     * @param privateBrowsing whether this is a private browsing instance of WebView.
     * @param isAccessFromFileURLsGrantedByDefault passed to ContentViewCore.initialize.
     */
    public AwContents(ViewGroup containerView,
        ContentViewCore.InternalAccessDelegate internalAccessAdapter,
        ContentViewCore contentViewCore, AwContentsClient contentsClient,
        NativeWindow nativeWindow, boolean privateBrowsing,
        boolean isAccessFromFileURLsGrantedByDefault) {
      mNativeAwContents = nativeInit(contentsClient.getWebContentsDelegate(), privateBrowsing);
      mContentViewCore = contentViewCore;
      mContentsClient = contentsClient;
      mCleanupReference = new CleanupReference(this, new DestroyRunnable(mNativeAwContents));

      // TODO: upstream the needed ContentViewCore initialization method.
      // mContentViewCore.initialize(containerView, internalAccessAdapter, false,
      //     nativeGetWebContents(mNativeAwContents), nativeWindow,
      //     isAccessFromFileURLsGrantedByDefault);
      mContentViewCore.setContentViewClient(contentsClient);
    }

    public ContentViewCore getContentViewCore() {
        return mContentViewCore;
    }

    public void setIoThreadClient(AwContentsIoThreadClient ioThreadClient) {
        mIoThreadClient = ioThreadClient;
        nativeSetIoThreadClient(mNativeAwContents, mIoThreadClient);
    }

    public void destroy() {
        if (mContentViewCore != null) {
            mContentViewCore.destroy();
            mContentViewCore = null;
        }
        mCleanupReference.cleanupNow();
    }

    /**
     * @return load progress of the WebContents
     */
    public int getMostRecentProgress() {
        // WebContentsDelegateAndroid conveniently caches the most recent notified value for us.
        return mContentsClient.getWebContentsDelegate().getMostRecentProgress();
    }

    //--------------------------------------------------------------------------------------------
    //  WebView[Provider] method implementations (where not provided by ContentViewCore)
    //--------------------------------------------------------------------------------------------

    public void documentHasImages(Message message) {
        nativeDocumentHasImages(mNativeAwContents, message);
    }

    public void saveWebArchive(
            final String basename, boolean autoname, final ValueCallback<String> callback) {
        if (!autoname) {
            saveWebArchiveInternal(basename, callback);
            return;
        }
        // If auto-generating the file name, handle the name generation on a background thread
        // as it will require I/O access for checking whether previous files existed.
        new AsyncTask<Void, Void, String>() {
            @Override
            protected String doInBackground(Void... params) {
                return generateArchiveAutoNamePath(getOriginalUrl(), basename);
            }

            @Override
            protected void onPostExecute(String result) {
                saveWebArchiveInternal(result, callback);
            }
        }.execute();
    }

    public String getOriginalUrl() {
        NavigationHistory history = mContentViewCore.getNavigationHistory();
        int currentIndex = history.getCurrentEntryIndex();
        if (currentIndex >= 0 && currentIndex < history.getEntryCount()) {
            return history.getEntryAtIndex(currentIndex).getOriginalUrl();
        }
        return null;
    }

    public String[] getHttpAuthUsernamePassword(String host, String realm) {
        return HttpAuthDatabase.getInstance(mContentViewCore.getContext())
                .getHttpAuthUsernamePassword(host, realm);
    }

    public void setHttpAuthUsernamePassword(String host, String realm, String username,
            String password) {
        HttpAuthDatabase.getInstance(mContentViewCore.getContext())
                .setHttpAuthUsernamePassword(host, realm, username, password);
    }

    //--------------------------------------------------------------------------------------------
    //  Methods called from native via JNI
    //--------------------------------------------------------------------------------------------

    @CalledByNative
    private static void onDocumentHasImagesResponse(boolean result, Message message) {
        message.arg1 = result ? 1 : 0;
        message.sendToTarget();
    }

    /** Callback for generateMHTML. */
    @CalledByNative
    private static void generateMHTMLCallback(
            String path, long size, ValueCallback<String> callback) {
        if (callback == null) return;
        callback.onReceiveValue(size < 0 ? null : path);
    }

    @CalledByNative
    private void onReceivedHttpAuthRequest(AwHttpAuthHandler handler, String host, String realm) {
        mContentsClient.onReceivedHttpAuthRequest(handler, host, realm);
    }

    // -------------------------------------------------------------------------------------------
    // Helper methods
    // -------------------------------------------------------------------------------------------

    private void saveWebArchiveInternal(String path, final ValueCallback<String> callback) {
        if (path == null) {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    callback.onReceiveValue(null);
                }
            });
        } else {
            nativeGenerateMHTML(mNativeAwContents, path, callback);
        }
    }

    /**
     * Try to generate a pathname for saving an MHTML archive. This roughly follows WebView's
     * autoname logic.
     */
    private static String generateArchiveAutoNamePath(String originalUrl, String baseName) {
        String name = null;
        if (originalUrl != null && !originalUrl.isEmpty()) {
            try {
                String path = new URL(originalUrl).getPath();
                int lastSlash = path.lastIndexOf('/');
                if (lastSlash > 0) {
                    name = path.substring(lastSlash + 1);
                } else {
                    name = path;
                }
            } catch (MalformedURLException e) {
                // If it fails parsing the URL, we'll just rely on the default name below.
            }
        }

        if (TextUtils.isEmpty(name)) name = "index";

        String testName = baseName + name + WEB_ARCHIVE_EXTENSION;
        if (!new File(testName).exists()) return testName;

        for (int i = 1; i < 100; i++) {
            testName = baseName + name + "-" + i + WEB_ARCHIVE_EXTENSION;
            if (!new File(testName).exists()) return testName;
        }

        Log.e(TAG, "Unable to auto generate archive name for path: " + baseName);
        return null;
    }

    //--------------------------------------------------------------------------------------------
    //  Native methods
    //--------------------------------------------------------------------------------------------

    private native int nativeInit(AwWebContentsDelegate webViewWebContentsDelegate,
            boolean privateBrowsing);
    private static native void nativeDestroy(int nativeAwContents);

    private native int nativeGetWebContents(int nativeAwContents);

    private native void nativeDocumentHasImages(int nativeAwContents, Message message);
    private native void nativeGenerateMHTML(
            int nativeAwContents, String path, ValueCallback<String> callback);

    private native void nativeSetIoThreadClient(int nativeAwContents,
            AwContentsIoThreadClient ioThreadClient);
}
