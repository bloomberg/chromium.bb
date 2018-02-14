// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import com.android.webview.chromium.SharedWebViewChromium;

import org.chromium.android_webview.AwContents;
import org.chromium.support_lib_boundary.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.VisualStateCallbackBoundaryInterface;
import org.chromium.support_lib_boundary.WebViewProviderBoundaryInterface;

import java.lang.reflect.InvocationHandler;

/**
 * Support library glue version of WebViewChromium.
 */
class SupportLibWebViewChromium implements WebViewProviderBoundaryInterface {
    private final SharedWebViewChromium mSharedWebViewChromium;

    public SupportLibWebViewChromium(SharedWebViewChromium sharedWebViewChromium) {
        mSharedWebViewChromium = sharedWebViewChromium;
    }

    @Override
    public void insertVisualStateCallback(long requestId, InvocationHandler callbackInvoHandler) {
        final VisualStateCallbackBoundaryInterface visualStateCallback =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        VisualStateCallbackBoundaryInterface.class, callbackInvoHandler);

        mSharedWebViewChromium.insertVisualStateCallback(
                requestId, new AwContents.VisualStateCallback() {
                    @Override
                    public void onComplete(long requestId) {
                        visualStateCallback.onComplete(requestId);
                    }
                });
    }
}
