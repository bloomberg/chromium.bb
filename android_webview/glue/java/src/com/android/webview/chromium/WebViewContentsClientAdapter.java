// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Picture;
import android.net.http.SslError;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.provider.Browser;
import android.util.Log;
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
import android.webkit.WebResourceResponse;
import android.webkit.WebResourceRequest;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import com.android.webview.chromium.WebViewDelegateFactory.WebViewDelegate;

import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwContentsClientBridge;
import org.chromium.android_webview.AwHttpAuthHandler;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.android_webview.JsPromptResultReceiver;
import org.chromium.android_webview.JsResultReceiver;
import org.chromium.android_webview.permission.AwPermissionRequest;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewClient;

import java.lang.ref.WeakReference;
import java.net.URISyntaxException;
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
public class WebViewContentsClientAdapter extends AwContentsClient {
    // TAG is chosen for consistency with classic webview tracing.
    private static final String TAG = "WebViewCallback";
    // Enables API callback tracing
    private static final boolean TRACE = false;
    // The WebView instance that this adapter is serving.
    private final WebView mWebView;
    // The Context to use. This is different from mWebView.getContext(), which should not be used.
    private final Context mContext;
    // The WebViewClient instance that was passed to WebView.setWebViewClient().
    private WebViewClient mWebViewClient;
    // The WebChromeClient instance that was passed to WebView.setContentViewClient().
    private WebChromeClient mWebChromeClient;
    // The listener receiving find-in-page API results.
    private WebView.FindListener mFindListener;
    // The listener receiving notifications of screen updates.
    private WebView.PictureListener mPictureListener;

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
                switch(msg.what) {
                    case NEW_WEBVIEW_CREATED:
                        WebView.WebViewTransport t = (WebView.WebViewTransport) msg.obj;
                        WebView newWebView = t.getWebView();
                        if (newWebView == mWebView) {
                            throw new IllegalArgumentException(
                                    "Parent WebView cannot host it's own popup window. Please " +
                                    "use WebSettings.setSupportMultipleWindows(false)");
                        }

                        if (newWebView != null && newWebView.copyBackForwardList().getSize() != 0) {
                            throw new IllegalArgumentException(
                                    "New WebView for popup window must not have been previously " +
                                    "navigated.");
                        }

                        WebViewChromium.completeWindowCreation(mWebView, newWebView);
                        break;
                    default:
                        throw new IllegalStateException();
                }
            }
        };

    }

    // WebViewClassic is coded in such a way that even if a null WebViewClient is set,
    // certain actions take place.
    // We choose to replicate this behavior by using a NullWebViewClient implementation (also known
    // as the Null Object pattern) rather than duplicating the WebViewClassic approach in
    // ContentView.
    static class NullWebViewClient extends WebViewClient {
        @Override
        public boolean shouldOverrideKeyEvent(WebView view, KeyEvent event) {
            // TODO: Investigate more and add a test case.
            // This is reflecting Clank's behavior.
            int keyCode = event.getKeyCode();
            return !ContentViewClient.shouldPropagateKey(keyCode);
        }

        @Override
        public boolean shouldOverrideUrlLoading(WebView view, String url) {
            Intent intent;
            // Perform generic parsing of the URI to turn it into an Intent.
            try {
                intent = Intent.parseUri(url, Intent.URI_INTENT_SCHEME);
            } catch (URISyntaxException ex) {
                Log.w(TAG, "Bad URI " + url + ": " + ex.getMessage());
                return false;
            }
            // Sanitize the Intent, ensuring web pages can not bypass browser
            // security (only access to BROWSABLE activities).
            intent.addCategory(Intent.CATEGORY_BROWSABLE);
            intent.setComponent(null);
            Intent selector = intent.getSelector();
            if (selector != null) {
                selector.addCategory(Intent.CATEGORY_BROWSABLE);
                selector.setComponent(null);
            }
            // Pass the package name as application ID so that the intent from the
            // same application can be opened in the same tab.
            intent.putExtra(Browser.EXTRA_APPLICATION_ID,
                    view.getContext().getPackageName());

            Context context = view.getContext();
            if (!(context instanceof Activity)) {
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            }

            try {
                context.startActivity(intent);
            } catch (ActivityNotFoundException ex) {
                Log.w(TAG, "No application can handle " + url);
                return false;
            }
            return true;
        }
    }

    void setWebViewClient(WebViewClient client) {
        if (client != null) {
            mWebViewClient = client;
        } else {
            mWebViewClient = new NullWebViewClient();
        }
    }

    void setWebChromeClient(WebChromeClient client) {
        mWebChromeClient = client;
    }

    void setDownloadListener(DownloadListener listener) {
        mDownloadListener = listener;
    }

    void setFindListener(WebView.FindListener listener) {
        mFindListener = listener;
    }

    void setPictureListener(WebView.PictureListener listener) {
        mPictureListener = listener;
    }

    //--------------------------------------------------------------------------------------------
    //                        Adapter for all the methods.
    //--------------------------------------------------------------------------------------------

    /**
     * @see AwContentsClient#getVisitedHistory
     */
    @Override
    public void getVisitedHistory(ValueCallback<String[]> callback) {
        TraceEvent.begin();
        if (mWebChromeClient != null) {
            if (TRACE) Log.d(TAG, "getVisitedHistory");
            mWebChromeClient.getVisitedHistory(callback);
        }
        TraceEvent.end();
    }

    /**
     * @see AwContentsClient#doUpdateVisiteHistory(String, boolean)
     */
    @Override
    public void doUpdateVisitedHistory(String url, boolean isReload) {
        TraceEvent.begin();
        if (TRACE) Log.d(TAG, "doUpdateVisitedHistory=" + url + " reload=" + isReload);
        mWebViewClient.doUpdateVisitedHistory(mWebView, url, isReload);
        TraceEvent.end();
    }

    /**
     * @see AwContentsClient#onProgressChanged(int)
     */
    @Override
    public void onProgressChanged(int progress) {
        TraceEvent.begin();
        if (mWebChromeClient != null) {
            if (TRACE) Log.d(TAG, "onProgressChanged=" + progress);
            mWebChromeClient.onProgressChanged(mWebView, progress);
        }
        TraceEvent.end();
    }

    private static class WebResourceRequestImpl implements WebResourceRequest {
        private final ShouldInterceptRequestParams mParams;

        public WebResourceRequestImpl(ShouldInterceptRequestParams params) {
            mParams = params;
        }

        @Override
        public Uri getUrl() {
            return Uri.parse(mParams.url);
        }

        @Override
        public boolean isForMainFrame() {
            return mParams.isMainFrame;
        }

        @Override
        public boolean hasGesture() {
            return mParams.hasUserGesture;
        }

        @Override
        public String getMethod() {
            return mParams.method;
        }

        @Override
        public Map<String, String> getRequestHeaders() {
            return mParams.requestHeaders;
        }
    }

    /**
     * @see AwContentsClient#shouldInterceptRequest(java.lang.String)
     */
    @Override
    public AwWebResourceResponse shouldInterceptRequest(ShouldInterceptRequestParams params) {
        TraceEvent.begin();
        if (TRACE) Log.d(TAG, "shouldInterceptRequest=" + params.url);
        WebResourceResponse response = mWebViewClient.shouldInterceptRequest(mWebView,
                new WebResourceRequestImpl(params));
        TraceEvent.end();
        if (response == null) return null;

        // AwWebResourceResponse should support null headers. b/16332774.
        Map<String, String> responseHeaders = response.getResponseHeaders();
        if (responseHeaders == null)
            responseHeaders = new HashMap<String, String>();

        return new AwWebResourceResponse(
                response.getMimeType(),
                response.getEncoding(),
                response.getData(),
                response.getStatusCode(),
                response.getReasonPhrase(),
                responseHeaders);
    }

    /**
     * @see AwContentsClient#shouldOverrideUrlLoading(java.lang.String)
     */
    @Override
    public boolean shouldOverrideUrlLoading(String url) {
        TraceEvent.begin();
        if (TRACE) Log.d(TAG, "shouldOverrideUrlLoading=" + url);
        boolean result = mWebViewClient.shouldOverrideUrlLoading(mWebView, url);
        TraceEvent.end();
        return result;
    }

    /**
     * @see AwContentsClient#onUnhandledKeyEvent(android.view.KeyEvent)
     */
    @Override
    public void onUnhandledKeyEvent(KeyEvent event) {
        TraceEvent.begin();
        if (TRACE) Log.d(TAG, "onUnhandledKeyEvent");
        mWebViewClient.onUnhandledKeyEvent(mWebView, event);
        TraceEvent.end();
    }

    /**
     * @see AwContentsClient#onConsoleMessage(android.webkit.ConsoleMessage)
     */
    @Override
    public boolean onConsoleMessage(ConsoleMessage consoleMessage) {
        TraceEvent.begin();
        boolean result;
        if (mWebChromeClient != null) {
            if (TRACE) Log.d(TAG, "onConsoleMessage: " + consoleMessage.message());
            result = mWebChromeClient.onConsoleMessage(consoleMessage);
            String message = consoleMessage.message();
            if (result && message != null && message.startsWith("[blocked]")) {
                Log.e(TAG, "Blocked URL: " + message);
            }
        } else {
            result = false;
        }
        TraceEvent.end();
        return result;
    }

    /**
     * @see AwContentsClient#onFindResultReceived(int,int,boolean)
     */
    @Override
    public void onFindResultReceived(int activeMatchOrdinal, int numberOfMatches,
            boolean isDoneCounting) {
        if (mFindListener == null) return;
        TraceEvent.begin();
        if (TRACE) Log.d(TAG, "onFindResultReceived");
        mFindListener.onFindResultReceived(activeMatchOrdinal, numberOfMatches, isDoneCounting);
        TraceEvent.end();
    }

    /**
     * @See AwContentsClient#onNewPicture(Picture)
     */
    @Override
    public void onNewPicture(Picture picture) {
        if (mPictureListener == null) return;
        TraceEvent.begin();
        if (TRACE) Log.d(TAG, "onNewPicture");
        mPictureListener.onNewPicture(mWebView, picture);
        TraceEvent.end();
    }

    @Override
    public void onLoadResource(String url) {
        TraceEvent.begin();
        if (TRACE) Log.d(TAG, "onLoadResource=" + url);
        mWebViewClient.onLoadResource(mWebView, url);
        TraceEvent.end();
    }

    @Override
    public boolean onCreateWindow(boolean isDialog, boolean isUserGesture) {
        Message m = mUiThreadHandler.obtainMessage(
                NEW_WEBVIEW_CREATED, mWebView.new WebViewTransport());
        TraceEvent.begin();
        boolean result;
        if (mWebChromeClient != null) {
            if (TRACE) Log.d(TAG, "onCreateWindow");
            result = mWebChromeClient.onCreateWindow(mWebView, isDialog, isUserGesture, m);
        } else {
            result = false;
        }
        TraceEvent.end();
        return result;
    }

    /**
     * @see AwContentsClient#onCloseWindow()
     */
    @Override
    public void onCloseWindow() {
        TraceEvent.begin();
        if (mWebChromeClient != null) {
            if (TRACE) Log.d(TAG, "onCloseWindow");
            mWebChromeClient.onCloseWindow(mWebView);
        }
        TraceEvent.end();
    }

    /**
     * @see AwContentsClient#onRequestFocus()
     */
    @Override
    public void onRequestFocus() {
        TraceEvent.begin();
        if (mWebChromeClient != null) {
            if (TRACE) Log.d(TAG, "onRequestFocus");
            mWebChromeClient.onRequestFocus(mWebView);
        }
        TraceEvent.end();
    }

    /**
     * @see AwContentsClient#onReceivedTouchIconUrl(String url, boolean precomposed)
     */
    @Override
    public void onReceivedTouchIconUrl(String url, boolean precomposed) {
        TraceEvent.begin();
        if (mWebChromeClient != null) {
            if (TRACE) Log.d(TAG, "onReceivedTouchIconUrl=" + url);
            mWebChromeClient.onReceivedTouchIconUrl(mWebView, url, precomposed);
        }
        TraceEvent.end();
    }

    /**
     * @see AwContentsClient#onReceivedIcon(Bitmap bitmap)
     */
    @Override
    public void onReceivedIcon(Bitmap bitmap) {
        TraceEvent.begin();
        if (mWebChromeClient != null) {
            if (TRACE) Log.d(TAG, "onReceivedIcon");
            mWebChromeClient.onReceivedIcon(mWebView, bitmap);
        }
        TraceEvent.end();
    }

    /**
     * @see ContentViewClient#onPageStarted(String)
     */
    @Override
    public void onPageStarted(String url) {
        TraceEvent.begin();
        if (TRACE) Log.d(TAG, "onPageStarted=" + url);
        mWebViewClient.onPageStarted(mWebView, url, mWebView.getFavicon());
        TraceEvent.end();
    }

    /**
     * @see ContentViewClient#onPageFinished(String)
     */
    @Override
    public void onPageFinished(String url) {
        TraceEvent.begin();
        if (TRACE) Log.d(TAG, "onPageFinished=" + url);
        mWebViewClient.onPageFinished(mWebView, url);
        TraceEvent.end();

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
        // To ensure backwards compatibility, we need to defer sending Picture updates
        // until onPageFinished has been invoked. This work is being done
        // upstream, and we can revert this hack when it lands.
        if (mPictureListener != null) {
            ThreadUtils.postOnUiThreadDelayed(new Runnable() {
                @Override
                public void run() {
                    UnimplementedWebViewApi.invoke();
                    if (mPictureListener != null) {
                        if (TRACE) Log.d(TAG, "onPageFinished-fake");
                        mPictureListener.onNewPicture(mWebView, new Picture());
                    }
                }
            }, 100);
        }
    }

    /**
     * @see ContentViewClient#onReceivedError(int,String,String)
     */
    @Override
    public void onReceivedError(int errorCode, String description, String failingUrl) {
        if (description == null || description.isEmpty()) {
            // ErrorStrings is @hidden, so we can't do this in AwContents.
            // Normally the net/ layer will set a valid description, but for synthesized callbacks
            // (like in the case for intercepted requests) AwContents will pass in null.
            description = mWebViewDelegate.getErrorString(mContext, errorCode);
        }
        TraceEvent.begin();
        if (TRACE) Log.d(TAG, "onReceivedError=" + failingUrl);
        mWebViewClient.onReceivedError(mWebView, errorCode, description, failingUrl);
        TraceEvent.end();
    }

    /**
     * @see ContentViewClient#onReceivedTitle(String)
     */
    @Override
    public void onReceivedTitle(String title) {
        TraceEvent.begin();
        if (mWebChromeClient != null) {
            if (TRACE) Log.d(TAG, "onReceivedTitle");
            mWebChromeClient.onReceivedTitle(mWebView, title);
        }
        TraceEvent.end();
    }


    /**
     * @see ContentViewClient#shouldOverrideKeyEvent(KeyEvent)
     */
    @Override
    public boolean shouldOverrideKeyEvent(KeyEvent event) {
        // The check below is reflecting Clank's behavior and is a workaround for http://b/7697782.
        // 1. The check for system key should be made in AwContents or ContentViewCore, before
        //    shouldOverrideKeyEvent() is called at all.
        // 2. shouldOverrideKeyEvent() should be called in onKeyDown/onKeyUp, not from
        //    dispatchKeyEvent().
        if (!ContentViewClient.shouldPropagateKey(event.getKeyCode())) return true;
        TraceEvent.begin();
        if (TRACE) Log.d(TAG, "shouldOverrideKeyEvent");
        boolean result = mWebViewClient.shouldOverrideKeyEvent(mWebView, event);
        TraceEvent.end();
        return result;
    }


    /**
     * @see ContentViewClient#onStartContentIntent(Context, String)
     * Callback when detecting a click on a content link.
     */
    // TODO: Delete this method when removed from base class.
    public void onStartContentIntent(Context context, String contentUrl) {
        TraceEvent.begin();
        if (TRACE) Log.d(TAG, "shouldOverrideUrlLoading=" + contentUrl);
        mWebViewClient.shouldOverrideUrlLoading(mWebView, contentUrl);
        TraceEvent.end();
    }

    @Override
    public void onGeolocationPermissionsShowPrompt(String origin,
            GeolocationPermissions.Callback callback) {
        TraceEvent.begin();
        if (mWebChromeClient != null) {
            if (TRACE) Log.d(TAG, "onGeolocationPermissionsShowPrompt");
            mWebChromeClient.onGeolocationPermissionsShowPrompt(origin, callback);
        }
        TraceEvent.end();
    }

    @Override
    public void onGeolocationPermissionsHidePrompt() {
        TraceEvent.begin();
        if (mWebChromeClient != null) {
            if (TRACE) Log.d(TAG, "onGeolocationPermissionsHidePrompt");
            mWebChromeClient.onGeolocationPermissionsHidePrompt();
        }
        TraceEvent.end();
    }

    @Override
    public void onPermissionRequest(AwPermissionRequest permissionRequest) {
        TraceEvent.begin();
        if (mWebChromeClient != null) {
            if (TRACE) Log.d(TAG, "onPermissionRequest");
            if (mOngoingPermissionRequests == null) {
                mOngoingPermissionRequests =
                    new WeakHashMap<AwPermissionRequest, WeakReference<PermissionRequestAdapter>>();
            }
            PermissionRequestAdapter adapter = new PermissionRequestAdapter(permissionRequest);
            mOngoingPermissionRequests.put(
                    permissionRequest, new WeakReference<PermissionRequestAdapter>(adapter));
            mWebChromeClient.onPermissionRequest(adapter);
        } else {
            // By default, we deny the permission.
            permissionRequest.deny();
        }
        TraceEvent.end();
    }

    @Override
    public void onPermissionRequestCanceled(AwPermissionRequest permissionRequest) {
        TraceEvent.begin();
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
        TraceEvent.end();
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
        TraceEvent.begin();
        if (mWebChromeClient != null) {
            final JsPromptResult res =
                    new JsPromptResultReceiverAdapter(receiver).getPromptResult();
            if (TRACE) Log.d(TAG, "onJsAlert");
            if (!mWebChromeClient.onJsAlert(mWebView, url, message, res)) {
                new JsDialogHelper(res, JsDialogHelper.ALERT, null, message, url)
                        .showDialog(mContext);
            }
        } else {
            receiver.cancel();
        }
        TraceEvent.end();
    }

    @Override
    public void handleJsBeforeUnload(String url, String message, JsResultReceiver receiver) {
        TraceEvent.begin();
        if (mWebChromeClient != null) {
            final JsPromptResult res =
                    new JsPromptResultReceiverAdapter(receiver).getPromptResult();
            if (TRACE) Log.d(TAG, "onJsBeforeUnload");
            if (!mWebChromeClient.onJsBeforeUnload(mWebView, url, message, res)) {
                new JsDialogHelper(res, JsDialogHelper.UNLOAD, null, message, url)
                        .showDialog(mContext);
            }
        } else {
            receiver.cancel();
        }
        TraceEvent.end();
    }

    @Override
    public void handleJsConfirm(String url, String message, JsResultReceiver receiver) {
        TraceEvent.begin();
        if (mWebChromeClient != null) {
            final JsPromptResult res =
                    new JsPromptResultReceiverAdapter(receiver).getPromptResult();
            if (TRACE) Log.d(TAG, "onJsConfirm");
            if (!mWebChromeClient.onJsConfirm(mWebView, url, message, res)) {
                new JsDialogHelper(res, JsDialogHelper.CONFIRM, null, message, url)
                        .showDialog(mContext);
            }
        } else {
            receiver.cancel();
        }
        TraceEvent.end();
    }

    @Override
    public void handleJsPrompt(String url, String message, String defaultValue,
            JsPromptResultReceiver receiver) {
        TraceEvent.begin();
        if (mWebChromeClient != null) {
            final JsPromptResult res =
                    new JsPromptResultReceiverAdapter(receiver).getPromptResult();
            if (TRACE) Log.d(TAG, "onJsPrompt");
            if (!mWebChromeClient.onJsPrompt(mWebView, url, message, defaultValue, res)) {
                new JsDialogHelper(res, JsDialogHelper.PROMPT, defaultValue, message, url)
                        .showDialog(mContext);
            }
        } else {
            receiver.cancel();
        }
        TraceEvent.end();
    }

    @Override
    public void onReceivedHttpAuthRequest(AwHttpAuthHandler handler, String host, String realm) {
        TraceEvent.begin();
        if (TRACE) Log.d(TAG, "onReceivedHttpAuthRequest=" + host);
        mWebViewClient.onReceivedHttpAuthRequest(mWebView,
                new AwHttpAuthHandlerAdapter(handler), host, realm);
        TraceEvent.end();
    }

    @Override
    public void onReceivedSslError(final ValueCallback<Boolean> callback, SslError error) {
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
        TraceEvent.begin();
        if (TRACE) Log.d(TAG, "onReceivedSslError");
        mWebViewClient.onReceivedSslError(mWebView, handler, error);
        TraceEvent.end();
    }

    private static class ClientCertRequestImpl extends ClientCertRequest {

        final private AwContentsClientBridge.ClientCertificateRequestCallback mCallback;
        final private String[] mKeyTypes;
        final private Principal[] mPrincipals;
        final private String mHost;
        final private int mPort;

        public ClientCertRequestImpl(
                AwContentsClientBridge.ClientCertificateRequestCallback callback,
                String[] keyTypes, Principal[] principals, String host, int port) {
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
            AwContentsClientBridge.ClientCertificateRequestCallback callback,
            String[] keyTypes, Principal[] principals, String host, int port) {
        if (TRACE) Log.d(TAG, "onReceivedClientCertRequest");
        TraceEvent.begin();
        final ClientCertRequestImpl request = new ClientCertRequestImpl(callback,
            keyTypes, principals, host, port);
        mWebViewClient.onReceivedClientCertRequest(mWebView, request);
        TraceEvent.end();
    }

    @Override
    public void onReceivedLoginRequest(String realm, String account, String args) {
        TraceEvent.begin();
        if (TRACE) Log.d(TAG, "onReceivedLoginRequest=" + realm);
        mWebViewClient.onReceivedLoginRequest(mWebView, realm, account, args);
        TraceEvent.end();
    }

    @Override
    public void onFormResubmission(Message dontResend, Message resend) {
        TraceEvent.begin();
        if (TRACE) Log.d(TAG, "onFormResubmission");
        mWebViewClient.onFormResubmission(mWebView, dontResend, resend);
        TraceEvent.end();
    }

    @Override
    public void onDownloadStart(String url,
                                String userAgent,
                                String contentDisposition,
                                String mimeType,
                                long contentLength) {
        if (mDownloadListener != null) {
            TraceEvent.begin();
            if (TRACE) Log.d(TAG, "onDownloadStart");
            mDownloadListener.onDownloadStart(url,
                                              userAgent,
                                              contentDisposition,
                                              mimeType,
                                              contentLength);
            TraceEvent.end();
        }
    }

    @Override
    public void showFileChooser(final ValueCallback<String[]> uploadFileCallback,
            final AwContentsClient.FileChooserParams fileChooserParams) {
        if (mWebChromeClient == null) {
            uploadFileCallback.onReceiveValue(null);
            return;
        }
        TraceEvent.begin();
        FileChooserParamsAdapter adapter = new FileChooserParamsAdapter(
                fileChooserParams, mContext);
        if (TRACE) Log.d(TAG, "showFileChooser");
        ValueCallback<Uri[]> callbackAdapter = new ValueCallback<Uri[]>() {
            private boolean mCompleted;
            @Override
            public void onReceiveValue(Uri[] uriList) {
                if (mCompleted) {
                    throw new IllegalStateException("showFileChooser result was already called");
                }
                mCompleted = true;
                String s[] = null;
                if (uriList != null) {
                    s = new String[uriList.length];
                    for(int i = 0; i < uriList.length; i++) {
                        s[i] = uriList[i].toString();
                    }
                }
                uploadFileCallback.onReceiveValue(s);
            }
        };

        // Invoke the new callback introduced in Lollipop. If the app handles
        // it, we're done here.
        if (mWebChromeClient.onShowFileChooser(mWebView, callbackAdapter, adapter)) {
            return;
        }

        // If the app did not handle it and we are running on Lollipop or newer, then
        // abort.
        if (mContext.getApplicationInfo().targetSdkVersion >=
                Build.VERSION_CODES.LOLLIPOP) {
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
                    throw new IllegalStateException("showFileChooser result was already called");
                }
                mCompleted = true;
                uploadFileCallback.onReceiveValue(
                        uri == null ? null : new String[] { uri.toString() });
            }
        };
        if (TRACE) Log.d(TAG, "openFileChooser");
        mWebChromeClient.openFileChooser(innerCallback, fileChooserParams.acceptTypes,
                fileChooserParams.capture ? "*" : "");
        TraceEvent.end();
    }

    @Override
    public void onScaleChangedScaled(float oldScale, float newScale) {
        TraceEvent.begin();
        if (TRACE) Log.d(TAG, " onScaleChangedScaled");
        mWebViewClient.onScaleChanged(mWebView, oldScale, newScale);
        TraceEvent.end();
    }

    @Override
    public void onShowCustomView(View view, CustomViewCallback cb) {
        TraceEvent.begin();
        if (mWebChromeClient != null) {
            if (TRACE) Log.d(TAG, "onShowCustomView");
            mWebChromeClient.onShowCustomView(view, cb);
        }
        TraceEvent.end();
    }

    @Override
    public void onHideCustomView() {
        TraceEvent.begin();
        if (mWebChromeClient != null) {
            if (TRACE) Log.d(TAG, "onHideCustomView");
            mWebChromeClient.onHideCustomView();
        }
        TraceEvent.end();
    }

    @Override
    protected View getVideoLoadingProgressView() {
        TraceEvent.begin();
        View result;
        if (mWebChromeClient != null) {
            if (TRACE) Log.d(TAG, "getVideoLoadingProgressView");
            result = mWebChromeClient.getVideoLoadingProgressView();
        } else {
            result = null;
        }
        TraceEvent.end();
        return result;
    }

    @Override
    public Bitmap getDefaultVideoPoster() {
        TraceEvent.begin();
        Bitmap result = null;
        if (mWebChromeClient != null) {
            if (TRACE) Log.d(TAG, "getDefaultVideoPoster");
            result = mWebChromeClient.getDefaultVideoPoster();
        }
        if (result == null) {
            // The ic_media_video_poster icon is transparent so we need to draw it on a gray
            // background.
            Bitmap poster = BitmapFactory.decodeResource(
                    mContext.getResources(),
                    R.drawable.ic_media_video_poster);
            result = Bitmap.createBitmap(poster.getWidth(), poster.getHeight(), poster.getConfig());
            result.eraseColor(Color.GRAY);
            Canvas canvas = new Canvas(result);
            canvas.drawBitmap(poster, 0f, 0f, null);
        }
        TraceEvent.end();
        return result;
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

    // TODO: Move to the upstream once the PermissionRequest is part of SDK.
    public static class PermissionRequestAdapter extends PermissionRequest {
        // TODO: Move the below definitions to AwPermissionRequest.
        private static long BITMASK_RESOURCE_VIDEO_CAPTURE = 1 << 1;
        private static long BITMASK_RESOURCE_AUDIO_CAPTURE = 1 << 2;
        private static long BITMASK_RESOURCE_PROTECTED_MEDIA_ID = 1 << 3;

        public static long toAwPermissionResources(String[] resources) {
            long result = 0;
            for (String resource : resources) {
                if (resource.equals(PermissionRequest.RESOURCE_VIDEO_CAPTURE))
                    result |= BITMASK_RESOURCE_VIDEO_CAPTURE;
                else if (resource.equals(PermissionRequest.RESOURCE_AUDIO_CAPTURE))
                    result |= BITMASK_RESOURCE_AUDIO_CAPTURE;
                else if (resource.equals(PermissionRequest.RESOURCE_PROTECTED_MEDIA_ID))
                    result |= BITMASK_RESOURCE_PROTECTED_MEDIA_ID;
            }
            return result;
        }

        private static String[] toPermissionResources(long resources) {
            ArrayList<String> result = new ArrayList<String>();
            if ((resources & BITMASK_RESOURCE_VIDEO_CAPTURE) != 0)
                result.add(PermissionRequest.RESOURCE_VIDEO_CAPTURE);
            if ((resources & BITMASK_RESOURCE_AUDIO_CAPTURE) != 0)
                result.add(PermissionRequest.RESOURCE_AUDIO_CAPTURE);
            if ((resources & BITMASK_RESOURCE_PROTECTED_MEDIA_ID) != 0)
                result.add(PermissionRequest.RESOURCE_PROTECTED_MEDIA_ID);
            String[] resource_array = new String[result.size()];
            return result.toArray(resource_array);
        }

        private AwPermissionRequest mAwPermissionRequest;
        private String[] mResources;

        public PermissionRequestAdapter(AwPermissionRequest awPermissionRequest) {
            assert awPermissionRequest != null;
            mAwPermissionRequest = awPermissionRequest;
        }

        @Override
        public Uri getOrigin() {
            return mAwPermissionRequest.getOrigin();
        }

        @Override
        public String[] getResources() {
            synchronized (this) {
                if (mResources == null) {
                    mResources = toPermissionResources(mAwPermissionRequest.getResources());
                }
                return mResources;
            }
        }

        @Override
        public void grant(String[] resources) {
            long requestedResource = mAwPermissionRequest.getResources();
            if ((requestedResource & toAwPermissionResources(resources)) == requestedResource)
                mAwPermissionRequest.grant();
            else
                mAwPermissionRequest.deny();
        }

        @Override
        public void deny() {
            mAwPermissionRequest.deny();
        }

    }
}
