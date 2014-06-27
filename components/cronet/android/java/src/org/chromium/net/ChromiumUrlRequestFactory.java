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
public class ChromiumUrlRequestFactory extends HttpUrlRequestFactory {
    private UrlRequestContext mRequestContext;

    @UsedByReflection("HttpUrlRequestFactory.java")
    public ChromiumUrlRequestFactory(
            Context context, HttpUrlRequestFactoryConfig config) {
        if (isEnabled()) {
            System.loadLibrary("cronet");
            mRequestContext = new UrlRequestContext(
                    context.getApplicationContext(), UserAgent.from(context),
                    config.toString());
        }
    }

    @Override
    public boolean isEnabled() {
        return Build.VERSION.SDK_INT >= 14;
    }

    @Override
    public String getName() {
        return "Chromium/" + UrlRequestContext.getVersion();
    }

    @Override
    public HttpUrlRequest createRequest(String url, int requestPriority,
            Map<String, String> headers, HttpUrlRequestListener listener) {
        return new ChromiumUrlRequest(mRequestContext, url, requestPriority,
                headers, listener);
    }

    @Override
    public HttpUrlRequest createRequest(String url, int requestPriority,
            Map<String, String> headers, WritableByteChannel channel,
            HttpUrlRequestListener listener) {
        return new ChromiumUrlRequest(mRequestContext, url, requestPriority,
                headers, channel, listener);
    }
}
