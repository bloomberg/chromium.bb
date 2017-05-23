// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Picture;
import android.net.Uri;
import android.net.http.SslError;
import android.os.Build;
import android.os.Handler;
import android.os.Message;
import android.view.KeyEvent;
import android.view.View;
import android.webkit.ClientCertRequest;
import android.webkit.ConsoleMessage;
import android.webkit.DownloadListener;
import android.webkit.GeolocationPermissions;
import android.webkit.JsDialogHelper;
import android.webkit.JsPromptResult;
import android.webkit.JsResult;
import android.webkit.PermissionRequest;
import android.webkit.SslErrorHandler;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebChromeClient.CustomViewCallback;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import com.android.webview.chromium.WebViewDelegateFactory.WebViewDelegate;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwContentsClientBridge;
import org.chromium.android_webview.AwHttpAuthHandler;
import org.chromium.android_webview.AwRenderProcessGoneDetail;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.android_webview.JsPromptResultReceiver;
import org.chromium.android_webview.JsResultReceiver;
import org.chromium.android_webview.permission.AwPermissionRequest;
import org.chromium.android_webview.permission.Resource;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;

import java.lang.ref.WeakReference;
import java.security.Principal;
import java.security.PrivateKey;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.WeakHashMap;

/**
 * An adapter class that forwards the callbacks from {@link ContentViewClient}
 * to the appropriate {@link WebViewClient} or {@link WebChromeClient}.
 *
 * An instance of this class is associated with one {@link WebViewChromium}
 * instance. A WebViewChromium is a WebView implementation provider (that is
 * android.webkit.WebView delegates all functionality to it) and has exactly
 * one corresponding {@link ContentView} instance.
 *
 * A {@link ContentViewClient} may be shared between multiple {@link ContentView}s,
 * and hence multiple WebViews. Many WebViewClient methods pass the source
 * WebView as an argument. This means that we either need to pass the
 * corresponding ContentView to the corresponding ContentViewClient methods,
 * or use an instance of ContentViewClientAdapter per WebViewChromium, to
 * allow the source WebView to be injected by ContentViewClientAdapter. We
 * choose the latter, because it makes for a cleaner design.
 */
@SuppressWarnings("deprecation")
// You shouldn't change TargetApi, please see how Android M API was added.
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
class WebViewContentsClientAdapter extends AwContentsClient {
    // TAG is chosen for consistency with classic webview tracing.
    private static final String TAG = "WebViewCallback";
    // Enables API callback tracing
    private static final boolean TRACE = false;
    // Default WebViewClient used to avoid null checks.
    private static WebViewClient sNullWebViewClient = new WebViewClient();
    // The WebView instance that this adapter is serving.
    protected final WebView mWebView;
    // The Context to use. This is different from mWebView.getContext(), which should not be used.
    private final Context mContext;
    // The WebViewClient instance that was passed to WebView.setWebViewClient().
    protected WebViewClient mWebViewClient = sNullWebViewClient;
    // The WebChromeClient instance that was passed to WebView.setContentViewClient().
    private WebChromeClient mWebChromeClient;
    // The listener receiving find-in-page API results.
    private WebView.FindListener mFindListener;
    // The listener receiving notifications of screen updates.
    private WebView.PictureListener mPictureListener;
    // Whether the picture listener is invalidate only (i.e. receives a null Picture)
    private boolean mPictureListenerInvalidateOnly;

    private WebViewDelegate mWebViewDelegate;

    private DownloadListener mDownloadListener;

    private Handler mUiThreadHandler;

    private static final int NEW_WEBVIEW_CREATED = 100;

    private WeakHashMap<AwPermissionRequest, WeakReference<PermissionRequestAdapter>>
            mOngoingPermissionRequests;

    /**
     * Adapter constructor.
     *
     * @param webView the {@link WebView} instance that this adapter is serving.
     */
    @SuppressLint("HandlerLeak")
    WebViewContentsClientAdapter(WebView webView, Context context,
            WebViewDelegate webViewDelegate) {
        if (webView == null || webViewDelegate == null) {
            throw new IllegalArgumentException("webView or delegate can't be null.");
        }

        if (context == null) {
            throw new IllegalArgumentException("context can't be null.");
        }

        mContext = context;
        mWebView = webView;
        mWebViewDelegate = webViewDelegate;
        setWebViewClient(null);

        mUiThreadHandler = new Handler() {
            @Override
            public void handleMessage(Message msg) {
                switch (msg.what) {
                    case NEW_WEBVIEW_CREATED:
                        WebView.WebViewTransport t = (WebView.WebViewTransport) msg.obj;
                        WebView newWebView = t.getWebView();
                        if (newWebView == mWebView) {
                            throw new IllegalArgumentException(
                                    "Parent WebView cannot host it's own popup window. Please "
                                    + "use WebSettings.setSupportMultipleWindows(false)");
                        }

                        if (newWebView != null && newWebView.copyBackForwardList().getSize() != 0) {
                            throw new IllegalArgumentException(
                                    "New WebView for popup window must not have been previously "
                                    + "navigated.");
                        }

                        WebViewChromium.completeWindowCreation(mWebView, newWebView);
                        break;
                    default:
                        throw new IllegalStateException();
                }
            }
        };
    }

    void setWebViewClient(WebViewClient client) {
        if (client != null) {
            mWebViewClient = client;
        } else {
            mWebViewClient = sNullWebViewClient;
        }
    }

    WebViewClient getWebViewClient() {
        return mWebViewClient;
    }

    void setWebChromeClient(WebChromeClient client) {
        mWebChromeClient = client;
    }

    WebChromeClient getWebChromeClient() {
        return mWebChromeClient;
    }

    void setDownloadListener(DownloadListener listener) {
        mDownloadListener = listener;
    }

    void setFindListener(WebView.FindListener listener) {
        mFindListener = listener;
    }

    void setPictureListener(WebView.PictureListener listener, boolean invalidateOnly) {
        mPictureListener = listener;
        mPictureListenerInvalidateOnly = invalidateOnly;
    }

    //--------------------------------------------------------------------------------------------
    //                        Adapter for all the methods.
    //--------------------------------------------------------------------------------------------

    /**
     * @see AwContentsClient#hasWebViewClient.
     */
    @Override
    public boolean hasWebViewClient() {
        return mWebViewClient != sNullWebViewClient;
    }

    /**
     * @see AwContentsClient#getVisitedHistory.
     */
    @Override
    public void getVisitedHistory(ValueCallback<String[]> callback) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.getVisitedHistory");
            if (mWebChromeClient != null) {
                if (TRACE) Log.d(TAG, "getVisitedHistory");
                mWebChromeClient.getVisitedHistory(callback);
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.getVisitedHistory");
        }
    }

    /**
     * @see AwContentsClient#doUpdateVisiteHistory(String, boolean)
     */
    @Override
    public void doUpdateVisitedHistory(String url, boolean isReload) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.doUpdateVisitedHistory");
            if (TRACE) Log.d(TAG, "doUpdateVisitedHistory=" + url + " reload=" + isReload);
            mWebViewClient.doUpdateVisitedHistory(mWebView, url, isReload);
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.doUpdateVisitedHistory");
        }
    }

    /**
     * @see AwContentsClient#onProgressChanged(int)
     */
    @Override
    public void onProgressChanged(int progress) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onProgressChanged");
            if (mWebChromeClient != null) {
                if (TRACE) Log.d(TAG, "onProgressChanged=" + progress);
                mWebChromeClient.onProgressChanged(mWebView, progress);
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onProgressChanged");
        }
    }

    private static class WebResourceRequestImpl implements WebResourceRequest {
        private final AwWebResourceRequest mRequest;

        public WebResourceRequestImpl(AwWebResourceRequest request) {
            mRequest = request;
        }

        @Override
        public Uri getUrl() {
            return Uri.parse(mRequest.url);
        }

        @Override
        public boolean isForMainFrame() {
            return mRequest.isMainFrame;
        }

        @Override
        public boolean hasGesture() {
            return mRequest.hasUserGesture;
        }

        @Override
        public String getMethod() {
            return mRequest.method;
        }

        @Override
        public Map<String, String> getRequestHeaders() {
            return mRequest.requestHeaders;
        }

        // TODO(mnaganov): Uncomment when we completely switch builds to the next API level.
        //@Override
        public boolean isRedirect() {
            return mRequest.isRedirect;
        }
    }

    @TargetApi(Build.VERSION_CODES.M)
    private static class WebResourceErrorImpl extends WebResourceError {
        private final AwWebResourceError mError;

        public WebResourceErrorImpl(AwWebResourceError error) {
            mError = error;
        }

        public int getErrorCode() {
            return mError.errorCode;
        }

        public CharSequence getDescription() {
            return mError.description;
        }
    }

    /**
     * @see AwContentsClient#shouldInterceptRequest(java.lang.String)
     */
    @Override
    public AwWebResourceResponse shouldInterceptRequest(AwWebResourceRequest request) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.shouldInterceptRequest");
            if (TRACE) Log.d(TAG, "shouldInterceptRequest=" + request.url);
            WebResourceResponse response = mWebViewClient.shouldInterceptRequest(mWebView,
                    new WebResourceRequestImpl(request));
            if (response == null) return null;

            // AwWebResourceResponse should support null headers. b/16332774.
            Map<String, String> responseHeaders = response.getResponseHeaders();
            if (responseHeaders == null) responseHeaders = new HashMap<String, String>();

            return new AwWebResourceResponse(
                    response.getMimeType(),
                    response.getEncoding(),
                    response.getData(),
                    response.getStatusCode(),
                    response.getReasonPhrase(),
                    responseHeaders);
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.shouldInterceptRequest");
        }
    }

    /**
     * @see AwContentsClient#shouldOverrideUrlLoading(AwContentsClient.AwWebResourceRequest)
     */
    @TargetApi(Build.VERSION_CODES.N)
    @Override
    public boolean shouldOverrideUrlLoading(AwContentsClient.AwWebResourceRequest request) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.shouldOverrideUrlLoading");
            if (TRACE) Log.d(TAG, "shouldOverrideUrlLoading=" + request.url);
            boolean result;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                result = mWebViewClient.shouldOverrideUrlLoading(
                        mWebView, new WebResourceRequestImpl(request));
            } else {
                result = mWebViewClient.shouldOverrideUrlLoading(mWebView, request.url);
            }
            return result;
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.shouldOverrideUrlLoading");
        }
    }

    /**
     * @see AwContentsClient#onUnhandledKeyEvent(android.view.KeyEvent)
     */
    @Override
    public void onUnhandledKeyEvent(KeyEvent event) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onUnhandledKeyEvent");
            if (TRACE) Log.d(TAG, "onUnhandledKeyEvent");
            mWebViewClient.onUnhandledKeyEvent(mWebView, event);
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onUnhandledKeyEvent");
        }
    }

    /**
     * @see AwContentsClient#onConsoleMessage(android.webkit.ConsoleMessage)
     */
    @Override
    public boolean onConsoleMessage(ConsoleMessage consoleMessage) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onConsoleMessage");
            boolean result;
            if (mWebChromeClient != null) {
                if (TRACE) Log.d(TAG, "onConsoleMessage: " + consoleMessage.message());
                result = mWebChromeClient.onConsoleMessage(consoleMessage);
            } else {
                result = false;
            }
            return result;
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onConsoleMessage");
        }
    }

    /**
     * @see AwContentsClient#onFindResultReceived(int,int,boolean)
     */
    @Override
    public void onFindResultReceived(int activeMatchOrdinal, int numberOfMatches,
            boolean isDoneCounting) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onFindResultReceived");
            if (mFindListener == null) return;
            if (TRACE) Log.d(TAG, "onFindResultReceived");
            mFindListener.onFindResultReceived(activeMatchOrdinal, numberOfMatches, isDoneCounting);
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onFindResultReceived");
        }
    }

    /**
     * @See AwContentsClient#onNewPicture(Picture)
     */
    @Override
    public void onNewPicture(Picture picture) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onNewPicture");
            if (mPictureListener == null) return;
            if (TRACE) Log.d(TAG, "onNewPicture");
            mPictureListener.onNewPicture(mWebView, picture);
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onNewPicture");
        }
    }

    @Override
    public void onLoadResource(String url) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onLoadResource");
            if (TRACE) Log.d(TAG, "onLoadResource=" + url);
            mWebViewClient.onLoadResource(mWebView, url);
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onLoadResource");
        }
    }

    @Override
    public boolean onCreateWindow(boolean isDialog, boolean isUserGesture) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onCreateWindow");
            Message m = mUiThreadHandler.obtainMessage(
                    NEW_WEBVIEW_CREATED, mWebView.new WebViewTransport());
            boolean result;
            if (mWebChromeClient != null) {
                if (TRACE) Log.d(TAG, "onCreateWindow");
                result = mWebChromeClient.onCreateWindow(mWebView, isDialog, isUserGesture, m);
            } else {
                result = false;
            }
            return result;
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onCreateWindow");
        }
    }

    /**
     * @see AwContentsClient#onCloseWindow()
     */
    @Override
    public void onCloseWindow() {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onCloseWindow");
            if (mWebChromeClient != null) {
                if (TRACE) Log.d(TAG, "onCloseWindow");
                mWebChromeClient.onCloseWindow(mWebView);
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onCloseWindow");
        }
    }

    /**
     * @see AwContentsClient#onRequestFocus()
     */
    @Override
    public void onRequestFocus() {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onRequestFocus");
            if (mWebChromeClient != null) {
                if (TRACE) Log.d(TAG, "onRequestFocus");
                mWebChromeClient.onRequestFocus(mWebView);
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onRequestFocus");
        }
    }

    /**
     * @see AwContentsClient#onReceivedTouchIconUrl(String url, boolean precomposed)
     */
    @Override
    public void onReceivedTouchIconUrl(String url, boolean precomposed) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onReceivedTouchIconUrl");
            if (mWebChromeClient != null) {
                if (TRACE) Log.d(TAG, "onReceivedTouchIconUrl=" + url);
                mWebChromeClient.onReceivedTouchIconUrl(mWebView, url, precomposed);
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onReceivedTouchIconUrl");
        }
    }

    /**
     * @see AwContentsClient#onReceivedIcon(Bitmap bitmap)
     */
    @Override
    public void onReceivedIcon(Bitmap bitmap) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onReceivedIcon");
            if (mWebChromeClient != null) {
                if (TRACE) Log.d(TAG, "onReceivedIcon");
                mWebChromeClient.onReceivedIcon(mWebView, bitmap);
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onReceivedIcon");
        }
    }

    /**
     * @see ContentViewClient#onPageStarted(String)
     */
    @Override
    public void onPageStarted(String url) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onPageStarted");
            if (TRACE) Log.d(TAG, "onPageStarted=" + url);
            mWebViewClient.onPageStarted(mWebView, url, mWebView.getFavicon());
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onPageStarted");
        }
    }

    /**
     * @see ContentViewClient#onPageFinished(String)
     */
    @Override
    public void onPageFinished(String url) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onPageFinished");
            if (TRACE) Log.d(TAG, "onPageFinished=" + url);
            mWebViewClient.onPageFinished(mWebView, url);

            // See b/8208948
            // This fakes an onNewPicture callback after onPageFinished to allow
            // CTS tests to run in an un-flaky manner. This is required as the
            // path for sending Picture updates in Chromium are decoupled from the
            // page loading callbacks, i.e. the Chrome compositor may draw our
            // content and send the Picture before onPageStarted or onPageFinished
            // are invoked. The CTS harness discards any pictures it receives before
            // onPageStarted is invoked, so in the case we get the Picture before that and
            // no further updates after onPageStarted, we'll fail the test by timing
            // out waiting for a Picture.
            if (mPictureListener != null) {
                ThreadUtils.postOnUiThreadDelayed(new Runnable() {
                    @Override
                    public void run() {
                        if (mPictureListener != null) {
                            if (TRACE) Log.d(TAG, "onPageFinished-fake");
                            mPictureListener.onNewPicture(mWebView,
                                    mPictureListenerInvalidateOnly ? null : new Picture());
                        }
                    }
                }, 100);
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onPageFinished");
        }
    }

    /**
     * @see ContentViewClient#onPageCommitVisible(String)
     */
    @Override
    public void onPageCommitVisible(String url) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return;
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onPageCommitVisible");
            if (TRACE) Log.d(TAG, "onPageCommitVisible=" + url);
            mWebViewClient.onPageCommitVisible(mWebView, url);
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onPageCommitVisible");
        }
    }

    /**
     * @see ContentViewClient#onReceivedError(int,String,String)
     */
    @Override
    public void onReceivedError(int errorCode, String description, String failingUrl) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) return;
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onReceivedError");
            if (description == null || description.isEmpty()) {
                // ErrorStrings is @hidden, so we can't do this in AwContents.  Normally the net/
                // layer will set a valid description, but for synthesized callbacks (like in the
                // case for intercepted requests) AwContents will pass in null.
                description = mWebViewDelegate.getErrorString(mContext, errorCode);
            }
            if (TRACE) Log.d(TAG, "onReceivedError=" + failingUrl);
            mWebViewClient.onReceivedError(
                    mWebView, errorCode, description, failingUrl);
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onReceivedError");
        }
    }

    /**
     * @see ContentViewClient#onReceivedError(AwWebResourceRequest,AwWebResourceError)
     */
    @Override
    public void onReceivedError2(AwWebResourceRequest request, AwWebResourceError error) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return;
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onReceivedError");
            if (error.description == null || error.description.isEmpty()) {
                // ErrorStrings is @hidden, so we can't do this in AwContents.  Normally the net/
                // layer will set a valid description, but for synthesized callbacks (like in the
                // case for intercepted requests) AwContents will pass in null.
                error.description = mWebViewDelegate.getErrorString(mContext, error.errorCode);
            }
            if (TRACE) Log.d(TAG, "onReceivedError=" + request.url);
            mWebViewClient.onReceivedError(mWebView, new WebResourceRequestImpl(request),
                    new WebResourceErrorImpl(error));
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onReceivedError");
        }
    }

    @Override
    public void onReceivedHttpError(AwWebResourceRequest request, AwWebResourceResponse response) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return;
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onReceivedHttpError");
            if (TRACE) Log.d(TAG, "onReceivedHttpError=" + request.url);
            mWebViewClient.onReceivedHttpError(mWebView, new WebResourceRequestImpl(request),
                    new WebResourceResponse(true, response.getMimeType(), response.getCharset(),
                            response.getStatusCode(), response.getReasonPhrase(),
                            response.getResponseHeaders(), response.getData()));
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onReceivedHttpError");
        }
    }

    /**
     * @see ContentViewClient#onReceivedTitle(String)
     */
    @Override
    public void onReceivedTitle(String title) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onReceivedTitle");
            if (mWebChromeClient != null) {
                if (TRACE) Log.d(TAG, "onReceivedTitle=\"" + title + "\"");
                mWebChromeClient.onReceivedTitle(mWebView, title);
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onReceivedTitle");
        }
    }

    /**
     * @see ContentViewClient#shouldOverrideKeyEvent(KeyEvent)
     */
    @Override
    public boolean shouldOverrideKeyEvent(KeyEvent event) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.shouldOverrideKeyEvent");
            if (TRACE) Log.d(TAG, "shouldOverrideKeyEvent");
            return mWebViewClient.shouldOverrideKeyEvent(mWebView, event);
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.shouldOverrideKeyEvent");
        }
    }

    /**
     * Returns true if a method with a given name and parameters is declared in a subclass
     * of a given baseclass.
     */
    private static <T> boolean isMethodDeclaredInSubClass(Class<T> baseClass,
            Class<? extends T> subClass, String name, Class<?>... parameterTypes) {
        try {
            return !subClass.getMethod(name, parameterTypes).getDeclaringClass().equals(baseClass);
        } catch (SecurityException e) {
            return false;
        } catch (NoSuchMethodException e) {
            return false;
        }
    }

    @Override
    public void onGeolocationPermissionsShowPrompt(String origin,
            GeolocationPermissions.Callback callback) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onGeolocationPermissionsShowPrompt");
            if (mWebChromeClient == null) {
                callback.invoke(origin, false, false);
                return;
            }
            if (!isMethodDeclaredInSubClass(WebChromeClient.class,
                                            mWebChromeClient.getClass(),
                                            "onGeolocationPermissionsShowPrompt",
                                            String.class,
                                            GeolocationPermissions.Callback.class)) {
                // This is only required for pre-M versions of android.
                callback.invoke(origin, false, false);
                return;
            }
            if (TRACE) Log.d(TAG, "onGeolocationPermissionsShowPrompt");
            mWebChromeClient.onGeolocationPermissionsShowPrompt(origin, callback);
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onGeolocationPermissionsShowPrompt");
        }
    }

    @Override
    public void onGeolocationPermissionsHidePrompt() {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onGeolocationPermissionsHidePrompt");
            if (mWebChromeClient != null) {
                if (TRACE) Log.d(TAG, "onGeolocationPermissionsHidePrompt");
                mWebChromeClient.onGeolocationPermissionsHidePrompt();
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onGeolocationPermissionsHidePrompt");
        }
    }

    @Override
    public void onPermissionRequest(AwPermissionRequest permissionRequest) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onPermissionRequest");
            if (mWebChromeClient != null) {
                if (TRACE) Log.d(TAG, "onPermissionRequest");
                if (mOngoingPermissionRequests == null) {
                    mOngoingPermissionRequests = new WeakHashMap<AwPermissionRequest,
                            WeakReference<PermissionRequestAdapter>>();
                }
                PermissionRequestAdapter adapter = new PermissionRequestAdapter(permissionRequest);
                mOngoingPermissionRequests.put(
                        permissionRequest, new WeakReference<PermissionRequestAdapter>(adapter));
                mWebChromeClient.onPermissionRequest(adapter);
            } else {
                // By default, we deny the permission.
                permissionRequest.deny();
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onPermissionRequest");
        }
    }

    @Override
    public void onPermissionRequestCanceled(AwPermissionRequest permissionRequest) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onPermissionRequestCanceled");
            if (mWebChromeClient != null && mOngoingPermissionRequests != null) {
                if (TRACE) Log.d(TAG, "onPermissionRequestCanceled");
                WeakReference<PermissionRequestAdapter> weakRef =
                        mOngoingPermissionRequests.get(permissionRequest);
                // We don't hold strong reference to PermissionRequestAdpater and don't expect the
                // user only holds weak reference to it either, if so, user has no way to call
                // grant()/deny(), and no need to be notified the cancellation of request.
                if (weakRef != null) {
                    PermissionRequestAdapter adapter = weakRef.get();
                    if (adapter != null) mWebChromeClient.onPermissionRequestCanceled(adapter);
                }
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onPermissionRequestCanceled");
        }
    }

    private static class JsPromptResultReceiverAdapter implements JsResult.ResultReceiver {
        private JsPromptResultReceiver mChromePromptResultReceiver;
        private JsResultReceiver mChromeResultReceiver;
        // We hold onto the JsPromptResult here, just to avoid the need to downcast
        // in onJsResultComplete.
        private final JsPromptResult mPromptResult = new JsPromptResult(this);

        public JsPromptResultReceiverAdapter(JsPromptResultReceiver receiver) {
            mChromePromptResultReceiver = receiver;
        }

        public JsPromptResultReceiverAdapter(JsResultReceiver receiver) {
            mChromeResultReceiver = receiver;
        }

        public JsPromptResult getPromptResult() {
            return mPromptResult;
        }

        @Override
        public void onJsResultComplete(JsResult result) {
            if (mChromePromptResultReceiver != null) {
                if (mPromptResult.getResult()) {
                    mChromePromptResultReceiver.confirm(mPromptResult.getStringResult());
                } else {
                    mChromePromptResultReceiver.cancel();
                }
            } else {
                if (mPromptResult.getResult()) {
                    mChromeResultReceiver.confirm();
                } else {
                    mChromeResultReceiver.cancel();
                }
            }
        }
    }

    @Override
    public void handleJsAlert(String url, String message, JsResultReceiver receiver) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.handleJsAlert");
            if (mWebChromeClient != null) {
                final JsPromptResult res =
                        new JsPromptResultReceiverAdapter(receiver).getPromptResult();
                if (TRACE) Log.d(TAG, "onJsAlert");
                if (!mWebChromeClient.onJsAlert(mWebView, url, message, res)) {
                    if (!showDefaultJsDialog(res, JsDialogHelper.ALERT, null, message, url)) {
                        receiver.cancel();
                    }
                }
            } else {
                receiver.cancel();
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.handleJsAlert");
        }
    }

    @Override
    public void handleJsBeforeUnload(String url, String message, JsResultReceiver receiver) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.handleJsBeforeUnload");
            if (mWebChromeClient != null) {
                final JsPromptResult res =
                        new JsPromptResultReceiverAdapter(receiver).getPromptResult();
                if (TRACE) Log.d(TAG, "onJsBeforeUnload");
                if (!mWebChromeClient.onJsBeforeUnload(mWebView, url, message, res)) {
                    if (!showDefaultJsDialog(res, JsDialogHelper.UNLOAD, null, message, url)) {
                        receiver.cancel();
                    }
                }
            } else {
                receiver.cancel();
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.handleJsBeforeUnload");
        }
    }

    @Override
    public void handleJsConfirm(String url, String message, JsResultReceiver receiver) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.handleJsConfirm");
            if (mWebChromeClient != null) {
                final JsPromptResult res =
                        new JsPromptResultReceiverAdapter(receiver).getPromptResult();
                if (TRACE) Log.d(TAG, "onJsConfirm");
                if (!mWebChromeClient.onJsConfirm(mWebView, url, message, res)) {
                    if (!showDefaultJsDialog(res, JsDialogHelper.CONFIRM, null, message, url)) {
                        receiver.cancel();
                    }
                }
            } else {
                receiver.cancel();
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.handleJsConfirm");
        }
    }

    @Override
    public void handleJsPrompt(String url, String message, String defaultValue,
            JsPromptResultReceiver receiver) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.handleJsPrompt");
            if (mWebChromeClient != null) {
                final JsPromptResult res =
                        new JsPromptResultReceiverAdapter(receiver).getPromptResult();
                if (TRACE) Log.d(TAG, "onJsPrompt");
                if (!mWebChromeClient.onJsPrompt(mWebView, url, message, defaultValue, res)) {
                    if (!showDefaultJsDialog(
                            res, JsDialogHelper.PROMPT, defaultValue, message, url)) {
                        receiver.cancel();
                    }
                }
            } else {
                receiver.cancel();
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.handleJsPrompt");
        }
    }

    /**
     * Try to show the default JS dialog and return whether the dialog was shown.
     */
    private boolean showDefaultJsDialog(JsPromptResult res, int jsDialogType, String defaultValue,
            String message, String url) {
        // Note we must unwrap the Context here due to JsDialogHelper only using instanceof to
        // check if a Context is an Activity.
        Context activityContext = AwContents.activityFromContext(mContext);
        if (activityContext == null) {
            Log.w(TAG, "Unable to create JsDialog without an Activity");
            return false;
        }
        new JsDialogHelper(res, jsDialogType, defaultValue, message, url)
            .showDialog(activityContext);
        return true;
    }

    @Override
    public void onReceivedHttpAuthRequest(AwHttpAuthHandler handler, String host, String realm) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onReceivedHttpAuthRequest");
            if (TRACE) Log.d(TAG, "onReceivedHttpAuthRequest=" + host);
            mWebViewClient.onReceivedHttpAuthRequest(
                    mWebView, new AwHttpAuthHandlerAdapter(handler), host, realm);
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onReceivedHttpAuthRequest");
        }
    }

    @Override
    public void onReceivedSslError(final ValueCallback<Boolean> callback, SslError error) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onReceivedSslError");
            SslErrorHandler handler = new SslErrorHandler() {
                @Override
                public void proceed() {
                    callback.onReceiveValue(true);
                }
                @Override
                public void cancel() {
                    callback.onReceiveValue(false);
                }
            };
            if (TRACE) Log.d(TAG, "onReceivedSslError");
            mWebViewClient.onReceivedSslError(mWebView, handler, error);
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onReceivedSslError");
        }
    }

    private static class ClientCertRequestImpl extends ClientCertRequest {
        private final AwContentsClientBridge.ClientCertificateRequestCallback mCallback;
        private final String[] mKeyTypes;
        private final Principal[] mPrincipals;
        private final String mHost;
        private final int mPort;

        public ClientCertRequestImpl(
                AwContentsClientBridge.ClientCertificateRequestCallback callback, String[] keyTypes,
                Principal[] principals, String host, int port) {
            mCallback = callback;
            mKeyTypes = keyTypes;
            mPrincipals = principals;
            mHost = host;
            mPort = port;
        }

        @Override
        public String[] getKeyTypes() {
            // This is already a copy of native argument, so return directly.
            return mKeyTypes;
        }

        @Override
        public Principal[] getPrincipals() {
            // This is already a copy of native argument, so return directly.
            return mPrincipals;
        }

        @Override
        public String getHost() {
            return mHost;
        }

        @Override
        public int getPort() {
            return mPort;
        }

        @Override
        public void proceed(final PrivateKey privateKey, final X509Certificate[] chain) {
            mCallback.proceed(privateKey, chain);
        }

        @Override
        public void ignore() {
            mCallback.ignore();
        }

        @Override
        public void cancel() {
            mCallback.cancel();
        }
    }

    @Override
    public void onReceivedClientCertRequest(
            AwContentsClientBridge.ClientCertificateRequestCallback callback, String[] keyTypes,
            Principal[] principals, String host, int port) {
        if (TRACE) Log.d(TAG, "onReceivedClientCertRequest");
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onReceivedClientCertRequest");
            final ClientCertRequestImpl request =
                    new ClientCertRequestImpl(callback, keyTypes, principals, host, port);
            mWebViewClient.onReceivedClientCertRequest(mWebView, request);
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onReceivedClientCertRequest");
        }
    }

    @Override
    public void onReceivedLoginRequest(String realm, String account, String args) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onReceivedLoginRequest");
            if (TRACE) Log.d(TAG, "onReceivedLoginRequest=" + realm);
            mWebViewClient.onReceivedLoginRequest(mWebView, realm, account, args);
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onReceivedLoginRequest");
        }
    }

    @Override
    public void onFormResubmission(Message dontResend, Message resend) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onFormResubmission");
            if (TRACE) Log.d(TAG, "onFormResubmission");
            mWebViewClient.onFormResubmission(mWebView, dontResend, resend);
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onFormResubmission");
        }
    }

    @Override
    public void onDownloadStart(
            String url,
            String userAgent,
            String contentDisposition,
            String mimeType,
            long contentLength) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onDownloadStart");
            if (mDownloadListener != null) {
                if (TRACE) Log.d(TAG, "onDownloadStart");
                mDownloadListener.onDownloadStart(
                        url, userAgent, contentDisposition, mimeType, contentLength);
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onDownloadStart");
        }
    }

    @Override
    public void showFileChooser(final ValueCallback<String[]> uploadFileCallback,
            final AwContentsClient.FileChooserParamsImpl fileChooserParams) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.showFileChooser");
            if (mWebChromeClient == null) {
                uploadFileCallback.onReceiveValue(null);
                return;
            }
            if (TRACE) Log.d(TAG, "showFileChooser");
            ValueCallback<Uri[]> callbackAdapter = new ValueCallback<Uri[]>() {
                private boolean mCompleted;
                @Override
                public void onReceiveValue(Uri[] uriList) {
                    if (mCompleted) {
                        throw new IllegalStateException(
                                "showFileChooser result was already called");
                    }
                    mCompleted = true;
                    String s[] = null;
                    if (uriList != null) {
                        s = new String[uriList.length];
                        for (int i = 0; i < uriList.length; i++) {
                            s[i] = uriList[i].toString();
                        }
                    }
                    uploadFileCallback.onReceiveValue(s);
                }
            };

            // Invoke the new callback introduced in Lollipop. If the app handles
            // it, we're done here.
            if (mWebChromeClient.onShowFileChooser(mWebView, callbackAdapter, fileChooserParams)) {
                return;
            }

            // If the app did not handle it and we are running on Lollipop or newer, then
            // abort.
            if (mContext.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.LOLLIPOP) {
                uploadFileCallback.onReceiveValue(null);
                return;
            }

            // Otherwise, for older apps, attempt to invoke the legacy (hidden) API for
            // backwards compatibility.
            ValueCallback<Uri> innerCallback = new ValueCallback<Uri>() {
                private boolean mCompleted;
                @Override
                public void onReceiveValue(Uri uri) {
                    if (mCompleted) {
                        throw new IllegalStateException(
                                "showFileChooser result was already called");
                    }
                    mCompleted = true;
                    uploadFileCallback.onReceiveValue(
                            uri == null ? null : new String[] {uri.toString()});
                }
            };
            if (TRACE) Log.d(TAG, "openFileChooser");
            mWebChromeClient.openFileChooser(
                    innerCallback,
                    fileChooserParams.getAcceptTypesString(),
                    fileChooserParams.isCaptureEnabled() ? "*" : "");
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.showFileChooser");
        }
    }

    @Override
    public void onScaleChangedScaled(float oldScale, float newScale) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onScaleChangedScaled");
            if (TRACE) Log.d(TAG, " onScaleChangedScaled");
            mWebViewClient.onScaleChanged(mWebView, oldScale, newScale);
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onScaleChangedScaled");
        }
    }

    @Override
    public void onShowCustomView(View view, CustomViewCallback cb) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onShowCustomView");
            if (mWebChromeClient != null) {
                if (TRACE) Log.d(TAG, "onShowCustomView");
                mWebChromeClient.onShowCustomView(view, cb);
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onShowCustomView");
        }
    }

    @Override
    public void onHideCustomView() {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onHideCustomView");
            if (mWebChromeClient != null) {
                if (TRACE) Log.d(TAG, "onHideCustomView");
                mWebChromeClient.onHideCustomView();
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onHideCustomView");
        }
    }

    @Override
    protected View getVideoLoadingProgressView() {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.getVideoLoadingProgressView");
            View result;
            if (mWebChromeClient != null) {
                if (TRACE) Log.d(TAG, "getVideoLoadingProgressView");
                result = mWebChromeClient.getVideoLoadingProgressView();
            } else {
                result = null;
            }
            return result;
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.getVideoLoadingProgressView");
        }
    }

    @Override
    public Bitmap getDefaultVideoPoster() {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.getDefaultVideoPoster");
            Bitmap result = null;
            if (mWebChromeClient != null) {
                if (TRACE) Log.d(TAG, "getDefaultVideoPoster");
                result = mWebChromeClient.getDefaultVideoPoster();
            }
            if (result == null) {
                // The ic_play_circle_outline_black_48dp icon is transparent so we need to draw it
                // on a gray background.
                Bitmap poster = BitmapFactory.decodeResource(
                        mContext.getResources(),
                        org.chromium.android_webview.R.drawable.ic_play_circle_outline_black_48dp);
                result = Bitmap.createBitmap(
                        poster.getWidth(), poster.getHeight(), poster.getConfig());
                result.eraseColor(Color.GRAY);
                Canvas canvas = new Canvas(result);
                canvas.drawBitmap(poster, 0f, 0f, null);
            }
            return result;
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.getDefaultVideoPoster");
        }
    }

    @Override
    public boolean onRenderProcessGone(AwRenderProcessGoneDetail detail) {
        return false;
    }

    // TODO: Move to upstream.
    private static class AwHttpAuthHandlerAdapter extends android.webkit.HttpAuthHandler {
        private AwHttpAuthHandler mAwHandler;

        public AwHttpAuthHandlerAdapter(AwHttpAuthHandler awHandler) {
            mAwHandler = awHandler;
        }

        @Override
        public void proceed(String username, String password) {
            if (username == null) {
                username = "";
            }

            if (password == null) {
                password = "";
            }
            mAwHandler.proceed(username, password);
        }

        @Override
        public void cancel() {
            mAwHandler.cancel();
        }

        @Override
        public boolean useHttpAuthUsernamePassword() {
            return mAwHandler.isFirstAttempt();
        }
    }

    /**
     * Type adaptation class for PermissionRequest.
     * TODO: Move to the upstream once the PermissionRequest is part of SDK.
     */
    public static class PermissionRequestAdapter extends PermissionRequest {

        private static long toAwPermissionResources(String[] resources) {
            long result = 0;
            for (String resource : resources) {
                if (resource.equals(PermissionRequest.RESOURCE_VIDEO_CAPTURE)) {
                    result |= Resource.VIDEO_CAPTURE;
                } else if (resource.equals(PermissionRequest.RESOURCE_AUDIO_CAPTURE)) {
                    result |= Resource.AUDIO_CAPTURE;
                } else if (resource.equals(PermissionRequest.RESOURCE_PROTECTED_MEDIA_ID)) {
                    result |= Resource.PROTECTED_MEDIA_ID;
                } else if (resource.equals(AwPermissionRequest.RESOURCE_MIDI_SYSEX)) {
                    result |= Resource.MIDI_SYSEX;
                }
            }
            return result;
        }

        private static String[] toPermissionResources(long resources) {
            ArrayList<String> result = new ArrayList<String>();
            if ((resources & Resource.VIDEO_CAPTURE) != 0) {
                result.add(PermissionRequest.RESOURCE_VIDEO_CAPTURE);
            }
            if ((resources & Resource.AUDIO_CAPTURE) != 0) {
                result.add(PermissionRequest.RESOURCE_AUDIO_CAPTURE);
            }
            if ((resources & Resource.PROTECTED_MEDIA_ID) != 0) {
                result.add(PermissionRequest.RESOURCE_PROTECTED_MEDIA_ID);
            }
            if ((resources & Resource.MIDI_SYSEX) != 0) {
                result.add(AwPermissionRequest.RESOURCE_MIDI_SYSEX);
            }
            String[] resource_array = new String[result.size()];
            return result.toArray(resource_array);
        }

        private AwPermissionRequest mAwPermissionRequest;
        private final String[] mResources;

        public PermissionRequestAdapter(AwPermissionRequest awPermissionRequest) {
            assert awPermissionRequest != null;
            mAwPermissionRequest = awPermissionRequest;
            mResources = toPermissionResources(mAwPermissionRequest.getResources());
        }

        @Override
        public Uri getOrigin() {
            return mAwPermissionRequest.getOrigin();
        }

        @Override
        public String[] getResources() {
            return mResources.clone();
        }

        @Override
        public void grant(String[] resources) {
            long requestedResource = mAwPermissionRequest.getResources();
            if ((requestedResource & toAwPermissionResources(resources)) == requestedResource) {
                mAwPermissionRequest.grant();
            } else {
                mAwPermissionRequest.deny();
            }
        }

        @Override
        public void deny() {
            mAwPermissionRequest.deny();
        }
    }
}
