// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.Intent;
import android.view.KeyEvent;

import org.chromium.base.Log;
import org.chromium.content.browser.ContentViewClient;

import java.net.URISyntaxException;

/**
 * ContentViewClient implementation for WebView
 */
public class AwContentViewClient extends ContentViewClient {
    private static final String TAG = "AwCVC";

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
        // Make sure that this URL is a valid scheme for this callback if interpreted as an intent,
        // even though we don't dispatch it as an intent here, because many WebView apps will once
        // it reaches them.
        String scheme = null;
        try {
            Intent intent = Intent.parseUri(contentUrl, Intent.URI_INTENT_SCHEME);
            scheme = intent.getScheme();
        } catch (URISyntaxException e) {
            // Just don't set the scheme, it will be rejected.
        }
        if (!isAcceptableContentIntentScheme(scheme)) {
            Log.w(TAG, "Invalid scheme for URI %s", contentUrl);
            return;
        }
        // Comes from WebViewImpl::detectContentOnTouch in Blink, so must be user-initiated, and
        // isn't a redirect.
        mAwContentsClient.shouldIgnoreNavigation(context, contentUrl, isMainFrame, true, false);
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
