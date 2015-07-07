// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.app.Activity;
import android.os.Bundle;

import android.webkit.ConsoleMessage;
import android.webkit.GeolocationPermissions;
import android.webkit.PermissionRequest;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * This activity is used for running layout tests using webview. The activity
 * creates a webview instance, loads url and captures console messages from
 * JavaScript until the test is finished.
 * provides a blocking callback.
 */
public class WebViewLayoutTestActivity extends Activity {

    private final StringBuilder mConsoleLog = new StringBuilder();
    private final Object mLock = new Object();
    private static final String TEST_FINISHED_SENTINEL = "TEST FINISHED";

    private WebView mWebView;
    private boolean mFinished = false;

    private static final String[] AUTOMATICALLY_GRANT =
            { PermissionRequest.RESOURCE_VIDEO_CAPTURE, PermissionRequest.RESOURCE_AUDIO_CAPTURE };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_webview);
        mWebView = (WebView) findViewById(R.id.webview);
        WebSettings settings = mWebView.getSettings();
        initializeSettings(settings);

        mWebView.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView webView, String url) {
                return false;
            }
        });

        mWebView.setWebChromeClient(new WebChromeClient() {
            @Override
            public void onGeolocationPermissionsShowPrompt(String origin,
                    GeolocationPermissions.Callback callback) {
                callback.invoke(origin, true, false);
            }

            @Override
            public void onPermissionRequest(PermissionRequest request) {
                request.grant(AUTOMATICALLY_GRANT);
            }

            @Override
            public boolean onConsoleMessage(ConsoleMessage consoleMessage) {
                // TODO(timvolodine): put log and warnings in separate string builders.
                mConsoleLog.append(consoleMessage.message() + "\n");
                if (consoleMessage.message().equals(TEST_FINISHED_SENTINEL)) {
                    finishTest();
                }
                return true;
            }
        });
    }

    public void waitForFinish(long timeout, TimeUnit unit) throws InterruptedException,
            TimeoutException {
        synchronized (mLock) {
            long deadline = System.currentTimeMillis() + unit.toMillis(timeout);
            while (!mFinished && System.currentTimeMillis() < deadline) {
                mLock.wait(deadline - System.currentTimeMillis());
            }
            if (!mFinished) {
                throw new TimeoutException("timeout");
            }
        }
    }

    public String getTestResult() {
        return mConsoleLog.toString();
    }

    public void loadUrl(String url) {
        mWebView.loadUrl(url);
        mWebView.requestFocus();
    }

    private void initializeSettings(WebSettings settings) {
        settings.setJavaScriptEnabled(true);
        settings.setGeolocationEnabled(true);
    }

    private void finishTest() {
        mFinished = true;
        synchronized (mLock) {
            mLock.notifyAll();
        }
    }
}
