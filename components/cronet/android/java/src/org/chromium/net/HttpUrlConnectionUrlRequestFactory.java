// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;

import java.nio.channels.WritableByteChannel;
import java.util.Map;

/**
 * Network request using {@link java.net.HttpURLConnection}.
 */
class HttpUrlConnectionUrlRequestFactory extends HttpUrlRequestFactory {

    private final Context mContext;

    public HttpUrlConnectionUrlRequestFactory(Context context) {
        mContext = context.getApplicationContext();
    }

    @Override
    protected boolean isEnabled() {
        return true;
    }

    @Override
    protected String getName() {
        return "HttpUrlConnection";
    }

    @Override
    protected HttpUrlRequest createRequest(String url, int requestPriority,
            Map<String, String> headers, HttpUrlRequestListener listener) {
        return new HttpUrlConnectionUrlRequest(mContext, url, requestPriority,
                headers, listener);
    }

    @Override
    protected HttpUrlRequest createRequest(String url, int requestPriority,
            Map<String, String> headers, WritableByteChannel channel,
            HttpUrlRequestListener listener) {
        return new HttpUrlConnectionUrlRequest(mContext, url, requestPriority,
                headers, channel, listener);
    }
}
