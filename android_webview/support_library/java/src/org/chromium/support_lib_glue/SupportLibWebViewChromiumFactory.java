// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.webkit.WebView;

import com.android.webview.chromium.WebkitToSharedGlueConverter;

import org.chromium.support_lib_boundary.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.WebViewProviderFactoryBoundaryInterface;

import java.lang.reflect.InvocationHandler;

/**
 * Support library glue version of WebViewChromiumFactoryProvider.
 */
class SupportLibWebViewChromiumFactory implements WebViewProviderFactoryBoundaryInterface {
    public SupportLibWebViewChromiumFactory() {}

    @Override
    public InvocationHandler createWebView(WebView webview) {
        return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                new SupportLibWebViewChromium(
                        WebkitToSharedGlueConverter.getSharedWebViewChromium(webview)));
    }
}
