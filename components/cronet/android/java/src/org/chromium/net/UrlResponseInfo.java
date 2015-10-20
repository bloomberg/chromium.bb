// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.TreeMap;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Contains basic information about a response. Included in {@link UrlRequestListener} callbacks.
 * Each {@link UrlRequestListener#onReceivedRedirect UrlRequestListener.onReceivedRedirect()}
 * callback gets a different copy of UrlResponseInfo describing a particular redirect response.
 */
public final class UrlResponseInfo {
    private final List<String> mResponseInfoUrlChain;
    private final int mHttpStatusCode;
    private final String mHttpStatusText;
    private final boolean mWasCached;
    private final String mNegotiatedProtocol;
    private final String mProxyServer;
    private final List<Map.Entry<String, String>> mAllHeadersList;
    private final AtomicLong mReceivedBytesCount;
    private Map<String, List<String>> mResponseHeaders;

    UrlResponseInfo(List<String> urlChain, int httpStatusCode, String httpStatusText,
            List<Map.Entry<String, String>> allHeadersList, boolean wasCached,
            String negotiatedProtocol, String proxyServer) {
        mResponseInfoUrlChain = Collections.unmodifiableList(urlChain);
        mHttpStatusCode = httpStatusCode;
        mHttpStatusText = httpStatusText;
        mAllHeadersList = Collections.unmodifiableList(allHeadersList);
        mWasCached = wasCached;
        mNegotiatedProtocol = negotiatedProtocol;
        mProxyServer = proxyServer;
        mReceivedBytesCount = new AtomicLong();
    }

    /**
     * Returns the URL the response is for. This is the URL after following
     * redirects, so it may not be the originally requested URL.
     * @return the URL the response is for.
     */
    public String getUrl() {
        return mResponseInfoUrlChain.get(mResponseInfoUrlChain.size() - 1);
    }

    /**
     * Returns the URL chain. The first entry is the origianlly requested URL;
     * the following entries are redirects followed.
     * @return the URL chain.
     */
    public List<String> getUrlChain() {
        return mResponseInfoUrlChain;
    }

    /**
     * Returns the HTTP status code. When a resource is retrieved from the cache,
     * whether it was revalidated or not, the original status code is returned.
     * @return the HTTP status code.
     */
    public int getHttpStatusCode() {
        return mHttpStatusCode;
    }

    /**
     * Returns the HTTP status text of the status line. For example, if the
     * request received a "HTTP/1.1 200 OK" response, this method returns "OK".
     * @return the HTTP status text of the status line.
     */
    public String getHttpStatusText() {
        return mHttpStatusText;
    }

    /**
     * Returns an unmodifiable list of response header field and value pairs.
     * The headers are in the same order they are received over the wire.
     * @return an unmodifiable list of response header field and value pairs.
     */
    public List<Map.Entry<String, String>> getAllHeadersAsList() {
        return mAllHeadersList;
    }

    /**
     * Returns an unmodifiable map of the response-header fields and values.
     * Each list of values for a single header field is in the same order they
     * were received over the wire.
     * @return an unmodifiable map of the response-header fields and values.
     */
    public Map<String, List<String>> getAllHeaders() {
        if (mResponseHeaders != null) {
            return mResponseHeaders;
        }
        Map<String, List<String>> map =
                new TreeMap<String, List<String>>(String.CASE_INSENSITIVE_ORDER);
        for (Map.Entry<String, String> entry : mAllHeadersList) {
            List<String> values = new ArrayList<String>();
            if (map.containsKey(entry.getKey())) {
                values.addAll(map.get(entry.getKey()));
            }
            values.add(entry.getValue());
            map.put(entry.getKey(), Collections.unmodifiableList(values));
        }
        mResponseHeaders = Collections.unmodifiableMap(map);
        return mResponseHeaders;
    }

    /**
     * Returns {@code true} if the response came from the cache, including
     * requests that were revalidated over the network before being retrieved
     * from the cache.
     * @return {@code true} if the response came from the cache, {@code false}
     *         otherwise.
     */
    public boolean wasCached() {
        return mWasCached;
    }

    /**
     * Returns the protocol (for example 'quic/1+spdy/3') negotiated with the server.
     * Returns an empty string if no protocol was negotiated, the protocol is
     * not known, or when using plain HTTP or HTTPS.
     * @return the protocol negotiated with the server.
     */
    // TODO(mef): Figure out what this returns in the cached case, both with
    // and without a revalidation request.
    public String getNegotiatedProtocol() {
        return mNegotiatedProtocol;
    }

    /**
     * Returns the proxy server that was used for the request.
     * @return the proxy server that was used for the request.
     */
    public String getProxyServer() {
        return mProxyServer;
    }

    /**
     * Returns a minimum count of bytes received from the network to process this
     * request. This count may ignore certain overheads (for example IP and TCP/UDP framing,
     * SSL handshake and framing, proxy handling). This count is taken prior to decompression
     * (for example GZIP and SDCH) and includes headers and data from all redirects.
     *
     * This value may change (even for one {@link UrlResponseInfo} instance) as the request
     * progresses until completion, when {@link UrlRequestListener#onSuccess},
     * {@link UrlRequestListener#onFailure}, or {@link UrlRequestListener#onCanceled} is called.
     */
    public long getReceivedBytesCount() {
        return mReceivedBytesCount.get();
    }

    @Override
    public String toString() {
        return String.format(Locale.ROOT, "UrlResponseInfo[%s]: urlChain = %s, "
                        + "httpStatus = %d %s, headers = %s, wasCached = %b, "
                        + "negotiatedProtocol = %s, proxyServer= %s, receivedBytesCount = %d",
                getUrl(), getUrlChain().toString(), getHttpStatusCode(), getHttpStatusText(),
                getAllHeadersAsList().toString(), wasCached(), getNegotiatedProtocol(),
                getProxyServer(), getReceivedBytesCount());
    }

    // Sets mReceivedBytesCount. Must not be called after request completion or cancellation.
    void setReceivedBytesCount(long currentReceivedBytesCount) {
        mReceivedBytesCount.set(currentReceivedBytesCount);
    }
}
