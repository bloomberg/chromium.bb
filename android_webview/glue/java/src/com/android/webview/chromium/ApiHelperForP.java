// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.TargetApi;
import android.os.Build;

import org.chromium.base.annotations.DoNotInline;

/**
 * Utility class to use new APIs that were added in P (API level 28). These need to exist in a
 * separate class so that Android framework can successfully verify WebView classes without
 * encountering the new APIs.
 */
@DoNotInline
@TargetApi(Build.VERSION_CODES.P)
public final class ApiHelperForP {
    private ApiHelperForP() {}

    /**
     * See {@link
     * TracingControllerAdapter#TracingControllerAdapter(WebViewChromiumFactoryProvider,
     * AwTracingController)}, which was added in N.
     */
    public static TracingControllerAdapter createTracingControllerAdapter(
            WebViewChromiumFactoryProvider provider, WebViewChromiumAwInit awInit) {
        return new TracingControllerAdapter(provider, awInit.getAwTracingController());
    }
}
