// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.util.Pair;

import java.util.List;
import java.util.Map;

/**
 * Contains basic information about a response. Sent to the embedder whenever
 * headers are received.
 */
public interface ResponseInfo {
    /**
     * Returns the URL the response is for. This is the URL after following
     * redirects, so it may not be the originally requested URL.
     * @return the URL the response is for.
     */
    String getUrl();

    /**
     * Returns the URL chain. The first entry is the origianlly requested URL;
     * the following entries are redirects followed.
     * @return the URL chain.
     */
    String[] getUrlChain();

    /**
     * Returns the HTTP status code. When a resource is retrieved from the cache,
     * whether it was revalidated or not, the original status code is returned.
     * @return the HTTP status code.
     */
    int getHttpStatusCode();

    /**
     * Returns the HTTP status text of the status line. For example, if the
     * request has a "HTTP/1.1 200 OK" response, this method returns "OK".
     * @return the HTTP status text of the status line.
     */
    String getHttpStatusText();

    /**
     * Returns an unmodifiable list of response header field and value pairs.
     * The headers are in the same order they are received over the wire.
     * @return an unmodifiable list of response header field and value pairs.
     */
    List<Pair<String, String>> getAllHeadersAsList();

    /**
     * Returns an unmodifiable map of the response-header fields and values.
     * Each list of values for a single header field is in the same order they
     * were received over the wire.
     * @return an unmodifiable map of the response-header fields and values.
     */
    Map<String, List<String>> getAllHeaders();

    /**
     * Returns {@code true} if the response came from the cache, including
     * requests that were revalidated over the network before being retrieved
     * from the cache.
     * @return {@code true} if the response came from the cache, {@code false}
     *         otherwise.
     */
    boolean wasCached();

    /**
     * Returns the protocol (e.g. "quic/1+spdy/3") negotiated with the server.
     * Returns an empty string if no protocol was negotiated, the protocol is
     * not known, or when using plain HTTP or HTTPS.
     * @return the protocol negotiated with the server.
     */
    // TODO(mef): Figure out what this returns in the cached case, both with
    // and without a revalidation request.
    String getNegotiatedProtocol();

    /**
     * Returns the proxy server that was used for the request.
     * @return the proxy server that was used for the request.
     */
    String getProxyServer();
};
