// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.SafeBrowsingResponse;
import android.webkit.ServiceWorkerWebSettings;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;

import org.chromium.android_webview.AwContentsClient.AwWebResourceError;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.AwSafeBrowsingResponse;
import org.chromium.android_webview.AwServiceWorkerSettings;
import org.chromium.android_webview.AwSettings;
import org.chromium.base.Callback;

/**
 * Class converting webkit objects to glue-objects shared between the webkit-glue and the support
 * library glue.
 * This class is used to minimize dependencies from the support-library-glue on the webkit-glue.
 */
public class WebkitToSharedGlueConverter {
    public static SharedWebViewChromium getSharedWebViewChromium(WebView webview) {
        WebViewChromium webviewChromium = (WebViewChromium) webview.getWebViewProvider();
        return webviewChromium.getSharedWebViewChromium();
    }

    public static AwSettings getSettings(WebSettings webSettings) {
        ContentSettingsAdapter contentSettingsAdapter = (ContentSettingsAdapter) webSettings;
        return contentSettingsAdapter.getAwSettings();
    }

    public static WebViewChromiumAwInit getGlobalAwInit() {
        return WebViewChromiumFactoryProvider.getSingleton().getAwInit();
    }

    public static AwServiceWorkerSettings getServiceWorkerSettings(
            ServiceWorkerWebSettings settings) {
        ServiceWorkerSettingsAdapter adapter = (ServiceWorkerSettingsAdapter) settings;
        return adapter.getAwSettings();
    }

    public static AwWebResourceRequest getWebResourceRequest(WebResourceRequest request) {
        WebResourceRequestAdapter adapter = (WebResourceRequestAdapter) request;
        return adapter.getAwResourceRequest();
    }

    public static AwWebResourceError getAwWebResourceError(WebResourceError error) {
        return ((WebResourceErrorAdapter) error).getAwWebResourceError();
    }

    public static Callback<AwSafeBrowsingResponse> getAwSafeBrowsingResponseCallback(
            SafeBrowsingResponse response) {
        return ((SafeBrowsingResponseAdapter) response).getAwSafeBrowsingResponseCallback();
    }
}
