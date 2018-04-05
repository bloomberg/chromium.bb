// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_callback_glue;

import android.support.annotation.Nullable;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.chromium.android_webview.AwContentsClient.AwWebResourceError;
import org.chromium.android_webview.AwSafeBrowsingResponse;
import org.chromium.android_webview.SafeBrowsingAction;
import org.chromium.base.Callback;
import org.chromium.support_lib_boundary.SafeBrowsingResponseBoundaryInterface;
import org.chromium.support_lib_boundary.WebResourceErrorBoundaryInterface;
import org.chromium.support_lib_boundary.WebViewClientBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;

import java.lang.reflect.InvocationHandler;

/**
 * Support library glue version of WebViewContentsClientAdapter.
 */
public class SupportLibWebViewContentsClientAdapter {
    private static final String WEBVIEW_CLIENT_COMPAT_NAME = "androidx.webkit.WebViewClientCompat";

    // If {@code null}, this indicates the WebViewClient is not a WebViewClientCompat. Otherwise,
    // this is a Proxy for the WebViewClientCompat.
    @Nullable
    private WebViewClientBoundaryInterface mWebViewClient;

    private static class SafeBrowsingResponseDelegate
            implements SafeBrowsingResponseBoundaryInterface {
        private Callback<AwSafeBrowsingResponse> mCallback;

        SafeBrowsingResponseDelegate(Callback<AwSafeBrowsingResponse> callback) {
            mCallback = callback;
        }

        @Override
        public void showInterstitial(boolean allowReporting) {
            mCallback.onResult(new AwSafeBrowsingResponse(
                    SafeBrowsingAction.SHOW_INTERSTITIAL, allowReporting));
        }

        @Override
        public void proceed(boolean report) {
            mCallback.onResult(new AwSafeBrowsingResponse(SafeBrowsingAction.PROCEED, report));
        }

        @Override
        public void backToSafety(boolean report) {
            mCallback.onResult(
                    new AwSafeBrowsingResponse(SafeBrowsingAction.BACK_TO_SAFETY, report));
        }
    };

    private static class WebResourceErrorDelegate implements WebResourceErrorBoundaryInterface {
        private AwWebResourceError mError;

        WebResourceErrorDelegate(AwWebResourceError error) {
            mError = error;
        }

        @Override
        public int getErrorCode() {
            return mError.errorCode;
        }

        @Override
        public CharSequence getDescription() {
            return mError.description;
        }
    };

    public SupportLibWebViewContentsClientAdapter(WebViewClient possiblyCompatClient) {
        mWebViewClient = convertCompatClient(possiblyCompatClient);
    }

    @Nullable
    private WebViewClientBoundaryInterface convertCompatClient(WebViewClient possiblyCompatClient) {
        if (!clientIsCompat(possiblyCompatClient)) return null;

        InvocationHandler handler =
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(possiblyCompatClient);

        return BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                WebViewClientBoundaryInterface.class, handler);
    }

    private boolean clientIsCompat(WebViewClient possiblyCompatClient) {
        try {
            Class compatClass = Class.forName(WEBVIEW_CLIENT_COMPAT_NAME, false,
                    possiblyCompatClient.getClass().getClassLoader());
            return compatClass.isInstance(possiblyCompatClient);
        } catch (ClassNotFoundException e) {
            // If WEBVIEW_CLIENT_COMPAT_NAME is not in the ClassLoader, then this cannot be an
            // instance of WebViewClientCompat.
            return false;
        }
    }

    /**
     * Indicates whether this client can handle the callback(s) assocated with {@param featureName}.
     * This should be called with the correct feature name before invoking the corresponding
     * callback, and the callback must not be called if this returns {@code false} for the feature.
     *
     * @param featureName the feature for the desired callback.
     * @return {@code true} if this client can handle the feature.
     */
    public boolean isFeatureAvailable(String featureName) {
        if (mWebViewClient == null) return false;
        // TODO(ntfschr): provide a real implementation, which consults the WebViewClientCompat.
        return true;
    }

    public void onPageCommitVisible(WebView webView, String url) {
        assert isFeatureAvailable(Features.VISUAL_STATE_CALLBACK);
        mWebViewClient.onPageCommitVisible(webView, url);
    }

    public void onReceivedError(
            WebView webView, WebResourceRequest request, final AwWebResourceError error) {
        assert isFeatureAvailable(Features.WEB_RESOURCE_ERROR);
        WebResourceErrorBoundaryInterface errorDelegate = new WebResourceErrorDelegate(error);
        InvocationHandler errorHandler =
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(errorDelegate);
        mWebViewClient.onReceivedError(webView, request, errorHandler);
    }

    public void onReceivedHttpError(
            WebView webView, WebResourceRequest request, WebResourceResponse response) {
        assert isFeatureAvailable(Features.RECEIVE_HTTP_ERROR);
        mWebViewClient.onReceivedHttpError(webView, request, response);
    }

    public void onSafeBrowsingHit(WebView webView, WebResourceRequest request, int threatType,
            Callback<AwSafeBrowsingResponse> callback) {
        assert isFeatureAvailable(Features.SAFE_BROWSING_HIT);
        SafeBrowsingResponseBoundaryInterface responseDelegate =
                new SafeBrowsingResponseDelegate(callback);
        InvocationHandler responseHandler =
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(responseDelegate);
        mWebViewClient.onSafeBrowsingHit(webView, request, threatType, responseHandler);
    }

    public boolean shouldOverrideUrlLoading(WebView webView, WebResourceRequest request) {
        assert isFeatureAvailable(Features.SHOULD_OVERRIDE_WITH_REDIRECTS);
        return mWebViewClient.shouldOverrideUrlLoading(webView, request);
    }
}
