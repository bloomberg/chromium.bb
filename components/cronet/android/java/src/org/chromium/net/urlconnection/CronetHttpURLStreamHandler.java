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
 * A {@link URLStreamHandler} that handles HTTP and HTTPS connections. One can use this class to
 * create {@link java.net.HttpURLConnection} instances implemented by Cronet; for example: <pre>
 *
 * CronetHttpURLStreamHandler streamHandler = new CronetHttpURLStreamHandler(myContext);
 * HttpURLConnection connection = (HttpURLConnection)streamHandler.openConnection(
 *         new URL("http://chromium.org"));</pre>
 * <b>Note:</b> Cronet's {@code HttpURLConnection} implementation is subject to some limitations
 * listed {@link CronetURLStreamHandlerFactory here}.
 */
public class CronetHttpURLStreamHandler extends URLStreamHandler {
    private final UrlRequestContext mUrlRequestContext;

    public CronetHttpURLStreamHandler(UrlRequestContext urlRequestContext) {
        mUrlRequestContext = urlRequestContext;
    }

    /**
     * Establishes a new connection to the resource specified by the {@link URL} {@code url}.
     * @return an {@link java.net.HttpURLConnection} instance implemented by Cronet.
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
     * Establishes a new connection to the resource specified by the {@link URL} {@code url}
     * using the given proxy.
     * @return an {@link java.net.HttpURLConnection} instance implemented by Cronet.
     */
    @Override
    public URLConnection openConnection(URL url, Proxy proxy) {
        throw new UnsupportedOperationException();
    }
}
