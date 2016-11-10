// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.view.KeyEvent;

import org.chromium.content.browser.ContentViewClient;

/**
 * ContentViewClient implementation for WebView
 */
public class AwContentViewClient extends ContentViewClient {
    private final AwContentsClient mAwContentsClient;
    private final AwSettings mAwSettings;
    private final AwContents mAwContents;
    private final Context mContext;

    public AwContentViewClient(AwContentsClient awContentsClient, AwSettings awSettings,
            AwContents awContents, Context context) {
        mAwContentsClient = awContentsClient;
        mAwSettings = awSettings;
        mAwContents = awContents;
        mContext = context;
    }

    @Override
    public void onBackgroundColorChanged(int color) {
        mAwContentsClient.onBackgroundColorChanged(color);
    }

    @Override
    public void onStartContentIntent(Context context, String contentUrl, boolean isMainFrame) {
        // Comes from WebViewImpl::detectContentOnTouch in Blink, so must be user-initiated, and
        // isn't a redirect.
        mAwContentsClient.shouldIgnoreNavigation(context, contentUrl, isMainFrame, true, false);
    }

    @Override
    public void onUpdateTitle(String title) {
        mAwContentsClient.updateTitle(title, true);
    }

    @Override
    public boolean shouldOverrideKeyEvent(KeyEvent event) {
        if (mAwContentsClient.hasWebViewClient()) {
            // The check below is reflecting Chrome's behavior and is a workaround for
            // http://b/7697782.
            if (!ContentViewClient.shouldPropagateKey(event.getKeyCode())) return true;
            return mAwContentsClient.shouldOverrideKeyEvent(event);
        }

        return super.shouldOverrideKeyEvent(event);
    }
}
