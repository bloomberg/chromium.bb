// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.graphics.Bitmap;
import android.net.http.SslCertificate;
import android.os.AsyncTask;
import android.os.Message;
import android.text.TextUtils;
import android.util.Log;
import android.view.ViewGroup;
import android.webkit.ValueCallback;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.component.navigation_interception.InterceptNavigationDelegate;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.LoadUrlParams;
import org.chromium.content.browser.NavigationHistory;
import org.chromium.content.common.CleanupReference;
import org.chromium.net.X509Util;
import org.chromium.ui.gfx.NativeWindow;

import java.io.File;
import java.net.MalformedURLException;
import java.net.URL;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;

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
    private InterceptNavigationDelegate mInterceptNavigationDelegate;
    // This can be accessed on any thread after construction. See AwContentsIoThreadClient.
    private final AwSettings mSettings;

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

    private class IoThreadClientImpl implements AwContentsIoThreadClient {
        // Called on the IO thread.
        @Override
        public InterceptedRequestData shouldInterceptRequest(String url) {
            return AwContents.this.mContentsClient.shouldInterceptRequest(url);
        }

        // Called on the IO thread.
        @Override
        public boolean shouldBlockContentUrls() {
            return !AwContents.this.mSettings.getAllowContentAccess();
        }

        // Called on the IO thread.
        @Override
        public boolean shouldBlockFileUrls() {
            return !AwContents.this.mSettings.getAllowFileAccess();
        }

        // Called on the IO thread.
        @Override
        public boolean shouldBlockNetworkLoads() {
            return AwContents.this.mSettings.getBlockNetworkLoads();
        }
    }

    private class InterceptNavigationDelegateImpl implements InterceptNavigationDelegate {
        @Override
        public boolean shouldIgnoreNavigation(String url, boolean isUserGestrue) {
            return AwContents.this.mContentsClient.shouldIgnoreNavigation(url);
        }
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

        mContentViewCore.initialize(containerView, internalAccessAdapter,
                nativeGetWebContents(mNativeAwContents), nativeWindow,
                isAccessFromFileURLsGrantedByDefault);
        mContentViewCore.setContentViewClient(contentsClient);
        mContentsClient.installWebContentsObserver(mContentViewCore);

        mSettings = new AwSettings(mContentViewCore.getContext());
        setIoThreadClient(new IoThreadClientImpl());
        setInterceptNavigationDelegate(new InterceptNavigationDelegateImpl());
    }

    public ContentViewCore getContentViewCore() {
        return mContentViewCore;
    }

    // Can be called from any thread.
    public AwSettings getSettings() {
        return mSettings;
    }

    public void setIoThreadClient(AwContentsIoThreadClient ioThreadClient) {
        mIoThreadClient = ioThreadClient;
        nativeSetIoThreadClient(mNativeAwContents, mIoThreadClient);
    }

    public void setInterceptNavigationDelegate(InterceptNavigationDelegate delegate) {
        mInterceptNavigationDelegate = delegate;
        nativeSetInterceptNavigationDelegate(mNativeAwContents, delegate);
    }

    public void destroy() {
        if (mContentViewCore != null) {
            mContentViewCore.destroy();
            mContentViewCore = null;
        }
        mCleanupReference.cleanupNow();
    }

    public int findAllSync(String searchString) {
        return nativeFindAllSync(mNativeAwContents, searchString);
    }

    public void findAllAsync(String searchString) {
        nativeFindAllAsync(mNativeAwContents, searchString);
    }

    public void findNext(boolean forward) {
        nativeFindNext(mNativeAwContents, forward);
    }

    public void clearMatches() {
        nativeClearMatches(mNativeAwContents);
    }

    /**
     * @return load progress of the WebContents.
     */
    public int getMostRecentProgress() {
        // WebContentsDelegateAndroid conveniently caches the most recent notified value for us.
        return mContentsClient.getWebContentsDelegate().getMostRecentProgress();
    }

    public Bitmap getFavicon() {
        // To be implemented.
        return null;
    }

    /**
     * Load url without fixing up the url string. Consumers of ContentView are responsible for
     * ensuring the URL passed in is properly formatted (i.e. the scheme has been added if left
     * off during user input).
     *
     * @param pararms Parameters for this load.
     */
    public void loadUrl(LoadUrlParams params) {
        if (params.getLoadUrlType() == LoadUrlParams.LOAD_TYPE_DATA &&
            !params.isBaseUrlDataScheme()) {
            // This allows data URLs with a non-data base URL access to file:///android_asset/ and
            // file:///android_res/ URLs. If AwSettings.getAllowFileAccess permits, it will also
            // allow access to file:// URLs (subject to OS level permission checks).
            params.setCanLoadLocalResources(true);
        }
        mContentViewCore.loadUrl(params);
    }

    //--------------------------------------------------------------------------------------------
    //  WebView[Provider] method implementations (where not provided by ContentViewCore)
    //--------------------------------------------------------------------------------------------

    /**
     * Clears the resource cache. Note that the cache is per-application, so this will clear the
     * cache for all WebViews used.
     *
     * @param includeDiskFiles if false, only the RAM cache is cleared
     */
    public void clearCache(boolean includeDiskFiles) {
        nativeClearCache(mNativeAwContents, includeDiskFiles);
    }

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

    /**
     * @see android.webkit.WebView#getCertificate()
     */
    public SslCertificate getCertificate() {
        byte[] derBytes = nativeGetCertificate(mNativeAwContents);
        if (derBytes == null) {
            return null;
        }

        try {
            X509Certificate x509Certificate =
                    X509Util.createCertificateFromBytes(derBytes);
            return new SslCertificate(x509Certificate);
        } catch (CertificateException e) {
            // Intentional fall through
            // A SSL related exception must have occured.  This shouldn't happen.
            Log.w(TAG, "Could not read certificate: " + e);
        } catch (KeyStoreException e) {
            // Intentional fall through
            // A SSL related exception must have occured.  This shouldn't happen.
            Log.w(TAG, "Could not read certificate: " + e);
        } catch (NoSuchAlgorithmException e) {
            // Intentional fall through
            // A SSL related exception must have occured.  This shouldn't happen.
            Log.w(TAG, "Could not read certificate: " + e);
        }
        return null;
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

    @CalledByNative
    public void onFindResultReceived(int activeMatchOrdinal, int numberOfMatches,
            boolean isDoneCounting) {
        mContentsClient.onFindResultReceived(activeMatchOrdinal, numberOfMatches, isDoneCounting);
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

    @CalledByNative
    private void handleJsAlert(String url, String message, JsResultReceiver receiver) {
        mContentsClient.handleJsAlert(url, message, receiver);
    }

    @CalledByNative
    private void handleJsBeforeUnload(String url, String message, JsResultReceiver receiver) {
        mContentsClient.handleJsBeforeUnload(url, message, receiver);
    }

    @CalledByNative
    private void handleJsConfirm(String url, String message, JsResultReceiver receiver) {
        mContentsClient.handleJsConfirm(url, message, receiver);
    }

    @CalledByNative
    private void handleJsPrompt(String url, String message, String defaultValue,
            JsPromptResultReceiver receiver) {
        mContentsClient.handleJsPrompt(url, message, defaultValue, receiver);
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
    private native void nativeSetInterceptNavigationDelegate(int nativeAwContents,
            InterceptNavigationDelegate navigationInterceptionDelegate);

    private native int nativeFindAllSync(int nativeAwContents, String searchString);
    private native void nativeFindAllAsync(int nativeAwContents, String searchString);
    private native void nativeFindNext(int nativeAwContents, boolean forward);
    private native void nativeClearMatches(int nativeAwContents);
    private native void nativeClearCache(int nativeAwContents, boolean includeDiskFiles);
    private native byte[] nativeGetCertificate(int nativeAwContents);
}
