// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.os.Build;

import org.chromium.base.UsedByReflection;

import java.nio.channels.WritableByteChannel;
import java.util.Map;

/**
 * Network request factory using the native http stack implementation.
 */
@UsedByReflection("HttpUrlRequestFactory.java")
public class ChromiumUrlRequestFactory extends HttpUrlRequestFactory {
    private ChromiumUrlRequestContext mRequestContext;

    @UsedByReflection("HttpUrlRequestFactory.java")
    public ChromiumUrlRequestFactory(
            Context context, HttpUrlRequestFactoryConfig config) {
        if (isEnabled()) {
            System.loadLibrary(config.libraryName());
            mRequestContext = new ChromiumUrlRequestContext(
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
        return "Chromium/" + ChromiumUrlRequestContext.getVersion();
    }

    @Override
    public ChromiumUrlRequest createRequest(String url, int requestPriority,
            Map<String, String> headers, HttpUrlRequestListener listener) {
        return new ChromiumUrlRequest(mRequestContext, url, requestPriority,
                headers, listener);
    }

    @Override
    public ChromiumUrlRequest createRequest(String url, int requestPriority,
            Map<String, String> headers, WritableByteChannel channel,
            HttpUrlRequestListener listener) {
        return new ChromiumUrlRequest(mRequestContext, url, requestPriority,
                headers, channel, listener);
    }

    public ChromiumUrlRequestContext getRequestContext() {
        return mRequestContext;
    }
}
