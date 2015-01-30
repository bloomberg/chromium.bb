// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import org.chromium.net.UrlRequestContext;

import java.io.IOException;
import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandler;

/**
 * An {@code URLStreamHandler} that handles http and https connections.
 */
public class CronetHttpURLStreamHandler extends URLStreamHandler {
    private final UrlRequestContext mUrlRequestContext;

    public CronetHttpURLStreamHandler(UrlRequestContext urlRequestContext) {
        mUrlRequestContext = urlRequestContext;
    }

    /**
     * Establishes a new connection to the resource specified by the URL url.
     */
    @Override
    public URLConnection openConnection(URL url) throws IOException {
        String protocol = url.getProtocol();
        if ("http".equals(protocol) || "https".equals(protocol)) {
            return new CronetHttpURLConnection(url, mUrlRequestContext);
        }
        throw new UnsupportedOperationException(
                "Unexpected protocol:" + protocol);
    }

    /**
     * Establishes a new connection to the resource specified by the URL url
     * using the given proxy.
     */
    @Override
    public URLConnection openConnection(URL url, Proxy proxy) {
        throw new UnsupportedOperationException();
    }
}
