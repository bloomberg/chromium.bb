// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.net.http.SslCertificate;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.text.TextUtils;
import android.util.Log;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.webkit.ValueCallback;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.LoadUrlParams;
import org.chromium.content.browser.NavigationHistory;
import org.chromium.content.common.CleanupReference;
import org.chromium.content.components.navigation_interception.InterceptNavigationDelegate;
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

    /**
     * WebKit hit test related data strcutre. These are used to implement
     * getHitTestResult, requestFocusNodeHref, requestImageRef methods in WebView.
     * All values should be updated together. The native counterpart is
     * AwHitTestData.
     */
    public static class HitTestData {
        // Used in getHitTestResult.
        public final int hitTestResultType;
        public final String hitTestResultExtraData;

        // Used in requestFocusNodeHref (all three) and requestImageRef (only imgSrc).
        public final String href;
        public final String anchorText;
        public final String imgSrc;

        private HitTestData(int type,
                            String extra,
                            String href,
                            String anchorText,
                            String imgSrc) {
            this.hitTestResultType = type;
            this.hitTestResultExtraData = extra;
            this.href = href;
            this.anchorText = anchorText;
            this.imgSrc = imgSrc;
        }
    }

    private int mNativeAwContents;
    private ContentViewCore mContentViewCore;
    private AwContentsClient mContentsClient;
    private AwContentsIoThreadClient mIoThreadClient;
    private InterceptNavigationDelegateImpl mInterceptNavigationDelegate;
    // This can be accessed on any thread after construction. See AwContentsIoThreadClient.
    private final AwSettings mSettings;
    private final IoThreadClientHandler mIoThreadClientHandler;

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

    private class IoThreadClientHandler extends Handler {
        public static final int MSG_SHOULD_INTERCEPT_REQUEST = 1;

        @Override
        public void handleMessage(Message msg) {
            switch(msg.what) {
                case MSG_SHOULD_INTERCEPT_REQUEST:
                    final String url = (String)msg.obj;
                    AwContents.this.mContentsClient.onLoadResource(url);
                    break;
                default:
                    throw new IllegalStateException(
                            "IoThreadClientHandler: unhandled message " + msg.what);
            }
        }
    }

    private class IoThreadClientImpl implements AwContentsIoThreadClient {
        // Called on the IO thread.
        @Override
        public InterceptedRequestData shouldInterceptRequest(final String url) {
            InterceptedRequestData interceptedRequestData =
                AwContents.this.mContentsClient.shouldInterceptRequest(url);
            if (interceptedRequestData == null) {
                mIoThreadClientHandler.sendMessage(
                        mIoThreadClientHandler.obtainMessage(
                            IoThreadClientHandler.MSG_SHOULD_INTERCEPT_REQUEST,
                            url));
            }
            return interceptedRequestData;
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
        private String mLastLoadUrlAddress;

        public void onUrlLoadRequested(String url) {
            mLastLoadUrlAddress = url;
        }

        @Override
        public boolean shouldIgnoreNavigation(String url, boolean isUserGestrue) {
            // If the embedder requested the load of a certain URL then querying whether to
            // override it is pointless.
            if (mLastLoadUrlAddress != null && mLastLoadUrlAddress.equals(url)) {
                // Support the case where the user clicks on a link that takes them back to the
                // same page.
                mLastLoadUrlAddress = null;
                return false;
            }
            return AwContents.this.mContentsClient.shouldIgnoreNavigation(url);
        }
    }

    /**
     * @param containerView the view-hierarchy item this object will be bound to.
     * @param internalAccessAdapter to access private methods on containerView.
     * @param contentsClient will receive API callbacks from this WebView Contents
     * @param privateBrowsing whether this is a private browsing instance of WebView.
     * @param isAccessFromFileURLsGrantedByDefault passed to ContentViewCore.initialize.
     */
    public AwContents(ViewGroup containerView,
            ContentViewCore.InternalAccessDelegate internalAccessAdapter,
            AwContentsClient contentsClient,
            NativeWindow nativeWindow, boolean privateBrowsing,
            boolean isAccessFromFileURLsGrantedByDefault) {
        // Note that ContentViewCore must be set up before AwContents, as ContentViewCore
        // setup performs process initialisation work needed by AwContents.
        mContentViewCore = new ContentViewCore(containerView.getContext(),
                ContentViewCore.PERSONALITY_VIEW);
        mNativeAwContents = nativeInit(contentsClient.getWebContentsDelegate(), privateBrowsing);
        mContentsClient = contentsClient;
        mCleanupReference = new CleanupReference(this, new DestroyRunnable(mNativeAwContents));
        mIoThreadClientHandler = new IoThreadClientHandler();

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

    private void setInterceptNavigationDelegate(InterceptNavigationDelegateImpl delegate) {
        mInterceptNavigationDelegate = delegate;
        nativeSetInterceptNavigationDelegate(mNativeAwContents, delegate);
    }

    public void destroy() {
        mContentViewCore.destroy();
        // We explicitly do not null out the mContentViewCore reference here
        // because ContentViewCore already has code to deal with the case
        // methods are called on it after it's been destroyed, and other
        // code relies on AwContents.getContentViewCore to return non-null.
        mCleanupReference.cleanupNow();
    }

    public static int getAwDrawGLFunction() {
        return nativeGetAwDrawGLFunction();
    }

    public int getAwDrawGLViewContext() {
        // Using the native pointer as the returned viewContext. This is matched by the
        // reinterpret_cast back to AwContents pointer in the native DrawGLFunction.
        return mNativeAwContents;
    }

    public boolean onPrepareDrawGL(Canvas canvas) {
        // TODO(joth): Ensure the HW path is setup and read any required params out of canvas.
        Log.e(TAG, "Not implemented: AwContents.onPrepareDrawGL()");

        // returning false will cause a fallback to SW path.
        return true;
    }

    public void onDraw(Canvas canvas) {
        // TODO(joth): Implement.
        Log.e(TAG, "Not implemented: AwContents.onDraw()");
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

        if (mInterceptNavigationDelegate != null) {
            // getUrl returns a sanitized address in the same format that will be used for
            // callbacks, so it's safe to use string comparison as an equality check later on.
            mInterceptNavigationDelegate.onUrlLoadRequested(mContentViewCore.getUrl());
        }
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

    /**
     * Method to return all hit test values relevant to public WebView API.
     * Note that this expose more data than needed for WebView.getHitTestResult.
     */
    public HitTestData getLastHitTestResult() {
        return nativeGetLastHitTestData(mNativeAwContents);
    }

    /**
     * @see android.webkit.WebView#requestFocusNodeHref()
     */
    public void requestFocusNodeHref(Message msg) {
        if (msg == null) {
            return;
        }

        HitTestData hitTestData = nativeGetLastHitTestData(mNativeAwContents);
        Bundle data = msg.getData();
        data.putString("url", hitTestData.href);
        data.putString("title", hitTestData.anchorText);
        data.putString("src", hitTestData.imgSrc);
        msg.setData(data);
        msg.sendToTarget();
    }

    /**
     * @see android.webkit.WebView#requestImageRef()
     */
    public void requestImageRef(Message msg) {
        if (msg == null) {
            return;
        }

        HitTestData hitTestData = nativeGetLastHitTestData(mNativeAwContents);
        Bundle data = msg.getData();
        data.putString("url", hitTestData.imgSrc);
        msg.setData(data);
        msg.sendToTarget();
    }

    /**
     * @see android.webkit.WebView#onTouchEvent()
     */
    public boolean onTouchEvent(MotionEvent event) {
        boolean rv = mContentViewCore.onTouchEvent(event);

        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
          int actionIndex = event.getActionIndex();

          // Note this will trigger IPC back to browser even if nothing is hit.
          nativeRequestNewHitTestDataAt(mNativeAwContents,
                                        Math.round(event.getX(actionIndex)),
                                        Math.round(event.getY(actionIndex)));
        }

        return rv;
    }

    /**
     * @see android.webkit.WebView#onHoverEvent()
     */
    public boolean onHoverEvent(MotionEvent event) {
        return mContentViewCore.onHoverEvent(event);
    }

    /**
     * @see android.webkit.WebView#onGenericMotionEvent()
     */
    public boolean onGenericMotionEvent(MotionEvent event) {
        return mContentViewCore.onGenericMotionEvent(event);
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

    @CalledByNative
    private static HitTestData createHitTestData(
            int type, String extra, String href, String anchorText, String imgSrc) {
        return new HitTestData(type, extra, href, anchorText, imgSrc);
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
    private static native int nativeGetAwDrawGLFunction();

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
    private native void nativeRequestNewHitTestDataAt(int nativeAwContents, int x, int y);
    private native HitTestData nativeGetLastHitTestData(int nativeAwContents);
}
