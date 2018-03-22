// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.content.Context;
import android.net.Uri;
import android.webkit.ValueCallback;
import android.webkit.WebView;

import com.android.webview.chromium.CallbackConverter;
import com.android.webview.chromium.SharedStatics;
import com.android.webview.chromium.WebViewChromiumAwInit;
import com.android.webview.chromium.WebkitToSharedGlueConverter;

import org.chromium.support_lib_boundary.StaticsBoundaryInterface;
import org.chromium.support_lib_boundary.SupportLibraryInfoBoundaryInterface;
import org.chromium.support_lib_boundary.WebViewProviderFactoryBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;

import java.lang.reflect.InvocationHandler;
import java.util.List;

/**
 * Support library glue version of WebViewChromiumFactoryProvider.
 */
class SupportLibWebViewChromiumFactory implements WebViewProviderFactoryBoundaryInterface {
    // SupportLibWebkitToCompatConverterAdapter
    private final InvocationHandler mCompatConverterAdapter;
    private final WebViewChromiumAwInit mAwInit;
    private final String[] mSupportLibrarySupportedFeatures;
    private final String[] mWebViewSupportedFeatures =
            new String[] {Features.VISUAL_STATE_CALLBACK};

    // Initialization guarded by mAwInit.getLock()
    private InvocationHandler mStatics;
    private InvocationHandler mServiceWorkerController;

    public SupportLibWebViewChromiumFactory(
            /* SupportLibraryInfo */ InvocationHandler supportLibraryInfo) {
        mCompatConverterAdapter = BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                new SupportLibWebkitToCompatConverterAdapter());
        mAwInit = WebkitToSharedGlueConverter.getGlobalAwInit();
        mSupportLibrarySupportedFeatures =
                BoundaryInterfaceReflectionUtil
                        .castToSuppLibClass(
                                SupportLibraryInfoBoundaryInterface.class, supportLibraryInfo)
                        .getSupportedFeatures();
    }

    @Override
    public InvocationHandler createWebView(WebView webview) {
        return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                new SupportLibWebViewChromium(
                        WebkitToSharedGlueConverter.getSharedWebViewChromium(webview)));
    }

    @Override
    public InvocationHandler getWebkitToCompatConverter() {
        return mCompatConverterAdapter;
    }

    private static class StaticsAdapter implements StaticsBoundaryInterface {
        private SharedStatics mSharedStatics;

        public StaticsAdapter(SharedStatics sharedStatics) {
            mSharedStatics = sharedStatics;
        }

        @Override
        public void initSafeBrowsing(Context context, ValueCallback<Boolean> callback) {
            mSharedStatics.initSafeBrowsing(context, CallbackConverter.fromValueCallback(callback));
        }

        @Override
        public void setSafeBrowsingWhitelist(List<String> hosts, ValueCallback<Boolean> callback) {
            mSharedStatics.setSafeBrowsingWhitelist(
                    hosts, CallbackConverter.fromValueCallback(callback));
        }

        @Override
        public Uri getSafeBrowsingPrivacyPolicyUrl() {
            return mSharedStatics.getSafeBrowsingPrivacyPolicyUrl();
        }
    }

    @Override
    public InvocationHandler getStatics() {
        synchronized (mAwInit.getLock()) {
            if (mStatics == null) {
                mStatics = BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new StaticsAdapter(
                                WebkitToSharedGlueConverter.getGlobalAwInit().getStatics()));
            }
        }
        return mStatics;
    }

    @Override
    public String[] getSupportedFeatures() {
        return mWebViewSupportedFeatures;
    }

    @Override
    public InvocationHandler getServiceWorkerController() {
        synchronized (mAwInit.getLock()) {
            if (mServiceWorkerController == null) {
                mServiceWorkerController =
                        BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                new SupportLibServiceWorkerControllerAdapter(
                                        mAwInit.getServiceWorkerController()));
            }
        }
        return mServiceWorkerController;
    }
}
