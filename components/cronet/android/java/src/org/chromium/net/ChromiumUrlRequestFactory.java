// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.os.Build;

import java.nio.channels.WritableByteChannel;
import java.util.Map;

/**
 * Network request factory using the native http stack implementation.
 */
@UsedByReflection("HttpUrlRequestFactory.java")
class ChromiumUrlRequestFactory extends HttpUrlRequestFactory {
    private static ChromiumUrlRequestContext sRequestContext;

    @UsedByReflection("HttpUrlRequestFactory.java")
    public ChromiumUrlRequestFactory(Context context) {
        if (sRequestContext == null && isEnabled()) {
            System.loadLibrary("cronet");
            sRequestContext = ChromiumUrlRequestContext.getInstance(context);
        }
    }

    @Override
    protected boolean isEnabled() {
        return Build.VERSION.SDK_INT >= 14;
    }

    @Override
    protected String getName() {
        return "Chromium/" + UrlRequestContext.getVersion();
    }

    @Override
    protected HttpUrlRequest createRequest(String url, int requestPriority,
            Map<String, String> headers, HttpUrlRequestListener listener) {
        return new ChromiumUrlRequest(sRequestContext, url, requestPriority,
                headers, listener);
    }

    @Override
    protected HttpUrlRequest createRequest(String url, int requestPriority,
            Map<String, String> headers, WritableByteChannel channel,
            HttpUrlRequestListener listener) {
        return new ChromiumUrlRequest(sRequestContext, url, requestPriority,
                headers, channel, listener);
    }
}
