// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.view.KeyEvent;

import org.chromium.content.browser.ContentViewClient;

/**
 * ContentViewClient implementation for WebView
 */
public class AwContentViewClient extends ContentViewClient {
    private static final String TAG = "AwCVC";

    private final AwContentsClient mAwContentsClient;
    private final AwSettings mAwSettings;
    private final AwContents mAwContents;

    public AwContentViewClient(
            AwContentsClient awContentsClient, AwSettings awSettings, AwContents awContents) {
        mAwContentsClient = awContentsClient;
        mAwSettings = awSettings;
        mAwContents = awContents;
    }

    @Override
    public boolean shouldOverrideKeyEvent(KeyEvent event) {
        if (mAwContentsClient.hasWebViewClient()) {
            // The check below is reflecting Chrome's behavior and is a workaround for
            // http://b/7697782.
            if (!shouldPropagateKey(event.getKeyCode())) return true;
            return mAwContentsClient.shouldOverrideKeyEvent(event);
        }

        return super.shouldOverrideKeyEvent(event);
    }
}
