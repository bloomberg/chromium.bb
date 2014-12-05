// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import android.content.Context;

import org.chromium.net.UrlRequestContext;
import org.chromium.net.UrlRequestContextConfig;

import java.net.URLStreamHandler;
import java.net.URLStreamHandlerFactory;

/**
 * An implementation of {@code URLStreamHandlerFactory} to handle http
 * and https traffic.
 */
public class CronetURLStreamHandlerFactory
        implements URLStreamHandlerFactory {
    private final Context mContext;
    private final UrlRequestContext mRequestContext;

    /**
     * Creates a {@link URLStreamHandlerFactory} to handle http and https
     * traffic.
     * @param context application context.
     * @param config If null the default config will be used.
     */
    public CronetURLStreamHandlerFactory(Context context,
            UrlRequestContextConfig config) {
        mContext = context;
        if (config == null) {
            config = new UrlRequestContextConfig();
            config.enableSPDY(true).enableQUIC(true);
        }
        mRequestContext = UrlRequestContext.createContext(mContext, config);
    }

    /**
     * Returns a {@link CronetHttpURLStreamHandler} for http and https, and
     * null for other protocols.
     */
    @Override
    public URLStreamHandler createURLStreamHandler(String protocol) {
        if ("http".equals(protocol) || "https".equals(protocol)) {
            return new CronetHttpURLStreamHandler(mRequestContext);
        }
        return null;
    }
}
