// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.webkit.CookieManager;
import android.webkit.WebSettings;
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
        setContentView(R.layout.activity_webview);

        WebView webView = (WebView) findViewById(R.id.webview);
        CookieManager.setAcceptFileSchemeCookies(true);
        WebSettings settings = webView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setUseWideViewPort(true);
        settings.setLoadWithOverviewMode(true);
        settings.setDomStorageEnabled(true);

        Intent intent = getIntent();
        String userAgentString = intent.getStringExtra("userAgent");
        if (userAgentString != null) {
            settings.setUserAgentString(userAgentString);
        }

        webView.setWebViewClient(new WebViewClient() {
                @Override
                public boolean shouldOverrideUrlLoading(WebView webView, String url) {
                    return false;
                }
        });

        webView.loadUrl("about:blank");
    }
}
