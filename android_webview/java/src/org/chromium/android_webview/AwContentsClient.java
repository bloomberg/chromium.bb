// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.graphics.Picture;
import android.net.http.SslError;
import android.os.Looper;
import android.os.Message;
import android.view.KeyEvent;
import android.view.View;
import android.webkit.ConsoleMessage;
import android.webkit.GeolocationPermissions;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;

import org.chromium.android_webview.permission.AwPermissionRequest;

import java.security.Principal;
import java.util.HashMap;

/**
 * Base-class that an AwContents embedder derives from to receive callbacks.
 * This extends ContentViewClient, as in many cases we want to pass-thru ContentViewCore
 * callbacks right to our embedder, and this setup facilities that.
 * For any other callbacks we need to make transformations of (e.g. adapt parameters
 * or perform filtering) we can provide final overrides for methods here, and then introduce
 * new abstract methods that the our own client must implement.
 * i.e.: all methods in this class should either be final, or abstract.
 */
public abstract class AwContentsClient {

    private final AwContentsClientCallbackHelper mCallbackHelper;

    // Last background color reported from the renderer. Holds the sentinal value INVALID_COLOR
    // if not valid.
    private int mCachedRendererBackgroundColor = INVALID_COLOR;

    private static final int INVALID_COLOR = 0;

    public AwContentsClient() {
        this(Looper.myLooper());
    }

    // Alllow injection of the callback thread, for testing.
    public AwContentsClient(Looper looper) {
        mCallbackHelper = new AwContentsClientCallbackHelper(looper, this);
    }

    final AwContentsClientCallbackHelper getCallbackHelper() {
        return mCallbackHelper;
    }

    final int getCachedRendererBackgroundColor() {
        assert isCachedRendererBackgroundColorValid();
        return mCachedRendererBackgroundColor;
    }

    final boolean isCachedRendererBackgroundColorValid() {
        return mCachedRendererBackgroundColor != INVALID_COLOR;
    }

    final void onBackgroundColorChanged(int color) {
        // Avoid storing the sentinal INVALID_COLOR (note that both 0 and 1 are both
        // fully transparent so this transpose makes no visible difference).
        mCachedRendererBackgroundColor = color == INVALID_COLOR ? 1 : color;
    }

    //--------------------------------------------------------------------------------------------
    //             WebView specific methods that map directly to WebViewClient / WebChromeClient
    //--------------------------------------------------------------------------------------------

    /**
     * Parameters for the {@link AwContentsClient#showFileChooser} method.
     */
    public static class FileChooserParams {
        public int mode;
        public String acceptTypes;
        public String title;
        public String defaultFilename;
        public boolean capture;
    }

    /**
     * Parameters for the {@link AwContentsClient#shouldInterceptRequest} method.
     */
    public static class ShouldInterceptRequestParams {
        // Url of the request.
        public String url;
        // Is this for the main frame or a child iframe?
        public boolean isMainFrame;
        // Was a gesture associated with the request? Don't trust can easily be spoofed.
        public boolean hasUserGesture;
        // Method used (GET/POST/OPTIONS)
        public String method;
        // Headers that would have been sent to server.
        public HashMap<String, String> requestHeaders;
    }

    public abstract void getVisitedHistory(ValueCallback<String[]> callback);

    public abstract void doUpdateVisitedHistory(String url, boolean isReload);

    public abstract void onProgressChanged(int progress);

    public abstract AwWebResourceResponse shouldInterceptRequest(
            ShouldInterceptRequestParams params);

    public abstract boolean shouldOverrideKeyEvent(KeyEvent event);

    public abstract boolean shouldOverrideUrlLoading(String url);

    public abstract void onLoadResource(String url);

    public abstract void onUnhandledKeyEvent(KeyEvent event);

    public abstract boolean onConsoleMessage(ConsoleMessage consoleMessage);

    public abstract void onReceivedHttpAuthRequest(AwHttpAuthHandler handler,
            String host, String realm);

    public abstract void onReceivedSslError(ValueCallback<Boolean> callback, SslError error);

    // TODO(sgurun): Make abstract once this has rolled in downstream.
    public void onReceivedClientCertRequest(
            final AwContentsClientBridge.ClientCertificateRequestCallback callback,
            final String[] keyTypes, final Principal[] principals, final String host,
            final int port) { }

    public abstract void onReceivedLoginRequest(String realm, String account, String args);

    public abstract void onFormResubmission(Message dontResend, Message resend);

    public abstract void onDownloadStart(String url, String userAgent, String contentDisposition,
            String mimeType, long contentLength);

    // TODO(joth): Make abstract once this has rolled in downstream.
    public /*abstract*/ void showFileChooser(ValueCallback<String[]> uploadFilePathsCallback,
            FileChooserParams fileChooserParams) { }

    public abstract void onGeolocationPermissionsShowPrompt(String origin,
            GeolocationPermissions.Callback callback);

    public abstract void onGeolocationPermissionsHidePrompt();

    // TODO(michaelbai): Change the abstract once merged
    public /*abstract*/ void onPermissionRequest(AwPermissionRequest awPermissionRequest) {}

    // TODO(michaelbai): Change the abstract once merged
    public /*abstract*/ void onPermissionRequestCanceled(
            AwPermissionRequest awPermissionRequest) {}

    public abstract void onScaleChangedScaled(float oldScale, float newScale);

    protected abstract void handleJsAlert(String url, String message, JsResultReceiver receiver);

    protected abstract void handleJsBeforeUnload(String url, String message,
            JsResultReceiver receiver);

    protected abstract void handleJsConfirm(String url, String message, JsResultReceiver receiver);

    protected abstract void handleJsPrompt(String url, String message, String defaultValue,
            JsPromptResultReceiver receiver);

    protected abstract boolean onCreateWindow(boolean isDialog, boolean isUserGesture);

    protected abstract void onCloseWindow();

    public abstract void onReceivedTouchIconUrl(String url, boolean precomposed);

    public abstract void onReceivedIcon(Bitmap bitmap);

    public abstract void onReceivedTitle(String title);

    protected abstract void onRequestFocus();

    protected abstract View getVideoLoadingProgressView();

    public abstract void onPageStarted(String url);

    public abstract void onPageFinished(String url);

    public abstract void onReceivedError(int errorCode, String description, String failingUrl);

    // TODO (michaelbai): Remove this method once the same method remove from
    // WebViewContentsClientAdapter.
    public void onShowCustomView(View view,
           int requestedOrientation, WebChromeClient.CustomViewCallback callback) {
    }

    // TODO (michaelbai): This method should be abstract, having empty body here
    // makes the merge to the Android easy.
    public void onShowCustomView(View view, WebChromeClient.CustomViewCallback callback) {
        onShowCustomView(view, ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED, callback);
    }

    public abstract void onHideCustomView();

    public abstract Bitmap getDefaultVideoPoster();

    //--------------------------------------------------------------------------------------------
    //                              Other WebView-specific methods
    //--------------------------------------------------------------------------------------------
    //
    public abstract void onFindResultReceived(int activeMatchOrdinal, int numberOfMatches,
            boolean isDoneCounting);

    /**
     * Called whenever there is a new content picture available.
     * @param picture New picture.
     */
    public abstract void onNewPicture(Picture picture);

}
