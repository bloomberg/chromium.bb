// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.view.KeyEvent;
import android.webkit.ConsoleMessage;

import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwHttpAuthHandler;
import org.chromium.android_webview.JsPromptResultReceiver;
import org.chromium.android_webview.JsResultReceiver;

/**
 * As a convience for tests that only care about specefic callbacks, this class provides
 * empty implementations of all abstract methods.
 */
class NullContentsClient extends AwContentsClient {
    @Override
    public boolean shouldOverrideUrlLoading(String url) {
        return false;
    }

    @Override
    public void onUnhandledKeyEvent(KeyEvent event) {
    }

    @Override
    public void onProgressChanged(int progress) {
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
}
