// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Picture;
import android.net.http.SslError;
import android.os.Message;
import android.view.KeyEvent;
import android.view.View;
import android.webkit.ConsoleMessage;
import android.webkit.GeolocationPermissions;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;

import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwHttpAuthHandler;
import org.chromium.android_webview.InterceptedRequestData;
import org.chromium.android_webview.JsPromptResultReceiver;
import org.chromium.android_webview.JsResultReceiver;

/**
 * As a convience for tests that only care about specefic callbacks, this class provides
 * empty implementations of all abstract methods.
 */
public class NullContentsClient extends AwContentsClient {
    @Override
    public boolean shouldIgnoreNavigation(String url) {
        return false;
    }

    @Override
    public void onUnhandledKeyEvent(KeyEvent event) {
    }

    @Override
    public void getVisitedHistory(ValueCallback<String[]> callback) {
    }

    @Override
    public void doUpdateVisitedHistory(String url, boolean isReload) {
    }

    @Override
    public void onProgressChanged(int progress) {
    }

    @Override
    public InterceptedRequestData shouldInterceptRequest(String url) {
        return null;
    }

    @Override
    public void onLoadResource(String url) {
    }

    @Override
    public boolean onConsoleMessage(ConsoleMessage consoleMessage) {
        return false;
    }

    @Override
    public void onReceivedHttpAuthRequest(AwHttpAuthHandler handler, String host, String realm) {
        handler.cancel();
    }

    @Override
    public void onReceivedSslError(ValueCallback<Boolean> callback, SslError error) {
        callback.onReceiveValue(false);
    }

    @Override
    public void onReceivedLoginRequest(String realm, String account, String args) {
    }

    @Override
    public void onGeolocationPermissionsShowPrompt(String origin,
            GeolocationPermissions.Callback callback) {
    }

    @Override
    public void onGeolocationPermissionsHidePrompt() {
    }

    @Override
    public void handleJsAlert(String url, String message, JsResultReceiver receiver) {
    }

    @Override
    public void handleJsBeforeUnload(String url, String message, JsResultReceiver receiver) {
    }

    @Override
    public void handleJsConfirm(String url, String message, JsResultReceiver receiver) {
    }

    @Override
    public void handleJsPrompt(
            String url, String message, String defaultValue, JsPromptResultReceiver receiver) {
    }

    @Override
    public void onFindResultReceived(int activeMatchOrdinal, int numberOfMatches,
            boolean isDoneCounting) {
    }

    @Override
    public void onNewPicture(Picture picture) {
    }

    @Override
    public void onPageStarted(String url) {
    }

    @Override
    public void onPageFinished(String url) {
    }

    @Override
    public void onReceivedError(int errorCode, String description, String failingUrl) {
    }

    @Override
    public void onFormResubmission(Message dontResend, Message resend) {
        dontResend.sendToTarget();
    }

    @Override
    public void onDownloadStart(String url,
                                String userAgent,
                                String contentDisposition,
                                String mimeType,
                                long contentLength) {
    }

    @Override
    public boolean onCreateWindow(boolean isDialog, boolean isUserGesture) {
        return false;
    }

    @Override
    public void onCloseWindow() {
    }

    @Override
    public void onRequestFocus() {
    }

    @Override
    public void onReceivedTouchIconUrl(String url, boolean precomposed) {
    }

    @Override
    public void onReceivedIcon(Bitmap bitmap) {
    }

    @Override
    public void onShowCustomView(View view,
           int requestedOrientation, WebChromeClient.CustomViewCallback callback) {
    }

    @Override
    public void onHideCustomView() {
    }

    @Override
    public void onScaleChangedScaled(float oldScale, float newScale) {
    }

    @Override
    protected View getVideoLoadingProgressView() {
        return null;
    }
}
