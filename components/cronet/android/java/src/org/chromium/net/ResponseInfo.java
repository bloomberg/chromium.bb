// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import java.net.URL;
import java.util.List;
import java.util.Map;

/**
 * Contains basic information about a response. Sent to the embedder whenever
 * headers are received.
 */
public abstract interface ResponseInfo {
    /**
     * Return the url the response is for (Not the original URL - after
     * redirects, it's the new URL).
     */
    URL getUrl();

    /**
     *
     * @return the url chain, including all redirects.  The originally
     * requested URL is first.
     */
    URL[] getUrlChain();

    /**
     * Returns the HTTP status code.
     */
    int getHttpStatusCode();

    /**
     * Returns an unmodifiable map of the response-header fields and values.
     * The null key is mapped to the HTTP status line for compatibility with
     * HttpUrlConnection.
     */
    Map<String, List<String>> getAllHeaders();

    /** True if the response came from the cache.  Requests that were
     * revalidated over the network before being retrieved from the cache are
     * considered cached.
     */
    boolean wasCached();

    /**
     *
     * @return
     */
    boolean wasFetchedOverSPDY();

    /**
     *
     * @return
     */
    boolean wasFetchedOverQUIC();
};
