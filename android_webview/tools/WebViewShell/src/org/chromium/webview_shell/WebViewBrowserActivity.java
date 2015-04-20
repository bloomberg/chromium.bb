// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.webkit.WebView;
import android.webkit.WebViewClient;

/**
 * This activity is designed for URL browsing testing of WebView. It takes a URL as an argument, and
 * displays the page.
 */
public class WebViewBrowserActivity extends Activity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setTitle(
                getResources().getString(R.string.title_activity_browser));
        setContentView(R.layout.activity_webview);
        WebView webView = (WebView) findViewById(R.id.webview);

        webView.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView webView, String url) {
                return false;
            }
        });

        String url = getUrlFromIntent(getIntent());
        webView.loadUrl(url);
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }

}
