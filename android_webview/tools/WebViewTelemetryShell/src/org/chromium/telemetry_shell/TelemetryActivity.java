// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.telemetry_shell;

import android.app.Activity;
import android.os.Bundle;
import android.webkit.CookieManager;
import android.webkit.WebView;
import android.webkit.WebViewClient;

/**
 * This activity is designed for Telemetry testing of WebView.
 */
public class TelemetryActivity extends Activity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setTitle(
                getResources().getString(R.string.title_activity_telemetry));
        setContentView(R.layout.activity_telemetry);

        WebView webView = (WebView) findViewById(R.id.webview);
        CookieManager.setAcceptFileSchemeCookies(true);
        webView.getSettings().setJavaScriptEnabled(true);

        webView.setWebViewClient(new WebViewClient() {
                @Override
                public boolean shouldOverrideUrlLoading(WebView webView, String url) {
                    return false;
                }
        });

        webView.loadUrl("about:blank");
    }
}
