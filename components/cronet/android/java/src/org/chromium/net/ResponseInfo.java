// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import java.util.List;
import java.util.Map;

/**
 * Contains basic information about a response. Sent to the embedder whenever
 * headers are received.
 */
public interface ResponseInfo {
    /**
     * @return the URL the response is for (Not the original URL - after
     * redirects, it's the new URL). Includes scheme, path, and query.
     */
    String getUrl();

    /**
     *
     * @return the url chain, including all redirects. The originally
     * requested URL is first.
     */
    String[] getUrlChain();

    /**
     * @return the HTTP status code.
     */
    int getHttpStatusCode();

    /**
     * @return an unmodifiable map of the response-header fields and values.
     * The null key is mapped to the HTTP status line for compatibility with
     * HttpUrlConnection.
     */
    Map<String, List<String>> getAllHeaders();

    /**
     * @return True if the response came from the cache. Requests that were
     * revalidated over the network before being retrieved from the cache are
     * considered cached. When a resource is retrieved from the cache
     * (Whether it was revalidated or not), getHttpStatusCode returns the
     * original status code.
     */
    boolean wasCached();

    /**
     * @return protocol (e.g. "quic/1+spdy/3") negotiated with server. Returns
     * empty string if no protocol was negotiated, or the protocol is not known.
     * Returns empty when using plain http or https.
     * TODO(mef): Figure out what this returns in the cached case, both with
     * and without a revalidation request.
     */
    String getNegotiatedProtocol();
};
