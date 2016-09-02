// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.TargetApi;
import android.net.Uri;
import android.os.Build;
import android.webkit.ServiceWorkerClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;

import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.AwServiceWorkerClient;
import org.chromium.android_webview.AwWebResourceResponse;

import java.util.HashMap;
import java.util.Map;

/**
 * An adapter class that forwards the callbacks from {@link AwServiceWorkerClient}
 * to the corresponding {@link ServiceWorkerClient}.
 */
@TargetApi(Build.VERSION_CODES.N)
public class ServiceWorkerClientAdapter extends AwServiceWorkerClient {

    private ServiceWorkerClient mServiceWorkerClient;

    public ServiceWorkerClientAdapter(ServiceWorkerClient client) {
        mServiceWorkerClient = client;
    }

    @Override
    public AwWebResourceResponse shouldInterceptRequest(AwWebResourceRequest request) {
        WebResourceResponse response = mServiceWorkerClient.shouldInterceptRequest(
                new WebResourceRequestImpl(request));
        if (response == null) return null;

        // AwWebResourceResponse should support null headers. b/16332774.
        Map<String, String> responseHeaders = response.getResponseHeaders();
        if (responseHeaders == null) responseHeaders = new HashMap<String, String>();

        return new AwWebResourceResponse(
                response.getMimeType(),
                response.getEncoding(),
                response.getData(),
                response.getStatusCode(),
                response.getReasonPhrase(),
                responseHeaders);
    }

    private static class WebResourceRequestImpl implements WebResourceRequest {
        private final AwWebResourceRequest mRequest;

        public WebResourceRequestImpl(AwWebResourceRequest request) {
            mRequest = request;
        }

        @Override
        public Uri getUrl() {
            return Uri.parse(mRequest.url);
        }

        @Override
        public boolean isForMainFrame() {
            return mRequest.isMainFrame;
        }

        @Override
        public boolean hasGesture() {
            return mRequest.hasUserGesture;
        }

        @Override
        public String getMethod() {
            return mRequest.method;
        }

        @Override
        public Map<String, String> getRequestHeaders() {
            return mRequest.requestHeaders;
        }

        // TODO(mnaganov): Uncomment when we completely switch builds to the next API level.
        //@Override
        public boolean isRedirect() {
            return mRequest.isRedirect;
        }
    }
}
