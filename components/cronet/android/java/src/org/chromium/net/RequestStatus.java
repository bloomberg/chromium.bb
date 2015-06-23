// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

/**
 * Request status values return by {@link UrlRequest#getStatus} API.
 */
public class RequestStatus {
    /**
     * This state indicates that the request is completed, canceled, or is not
     * started.
     */
    public static final int INVALID = -1;
    /**
     * This state corresponds to a resource load that has either not yet begun
     * or is idle waiting for the consumer to do something to move things along
     * (e.g., the consumer of an {@link UrlRequest} may not have called
     * {@UrlRequest#read} yet).
     */
    public static final int IDLE = 0;
    /**
     * When a socket pool group is below the maximum number of sockets allowed
     * per group, but a new socket cannot be created due to the per-pool socket
     * limit, this state is returned by all requests for the group waiting on an
     * idle connection, except those that may be serviced by a pending new
     * connection.
     */
    public static final int WAITING_FOR_STALLED_SOCKET_POOL = 1;
    /**
     * When a socket pool group has reached the maximum number of sockets
     * allowed per group, this state is returned for all requests that don't
     * have a socket, except those that correspond to a pending new connection.
     */
    public static final int WAITING_FOR_AVAILABLE_SOCKET = 2;
    /**
     * This state indicates that the URLRequest delegate has chosen to block
     * this request before it was sent over the network.
     */
    public static final int WAITING_FOR_DELEGATE = 3;
    /**
     * This state corresponds to a resource load that is blocked waiting for
     * access to a resource in the cache. If multiple requests are made for the
     * same resource, the first request will be responsible for writing (or
     * updating) the cache entry and the second request will be deferred until
     * the first completes. This may be done to optimize for cache reuse.
     */
    public static final int WAITING_FOR_CACHE = 4;
    /**
     * This state corresponds to a resource being blocked waiting for the
     * PAC script to be downloaded.
     */
    public static final int DOWNLOADING_PROXY_SCRIPT = 5;
    /**
     * This state corresponds to a resource load that is blocked waiting for a
     * proxy autoconfig script to return a proxy server to use.
     */
    public static final int RESOLVING_PROXY_FOR_URL = 6;
    /**
     * This state corresponds to a resource load that is blocked waiting for a
     * proxy autoconfig script to return a proxy server to use, but that proxy
     * script is busy resolving the IP address of a host.
     */
    public static final int RESOLVING_HOST_IN_PROXY_SCRIPT = 7;
    /**
     * This state indicates that we're in the process of establishing a tunnel
     * through the proxy server.
     */
    public static final int ESTABLISHING_PROXY_TUNNEL = 8;
    /**
     * This state corresponds to a resource load that is blocked waiting for a
     * host name to be resolved. This could either indicate resolution of the
     * origin server corresponding to the resource or to the host name of a
     * proxy server used to fetch the resource.
     */
    public static final int RESOLVING_HOST = 9;
    /**
     * This state corresponds to a resource load that is blocked waiting for a
     * TCP connection (or other network connection) to be established. HTTP
     * requests that reuse a keep-alive connection skip this state.
     */
    public static final int CONNECTING = 10;
    /**
     * This state corresponds to a resource load that is blocked waiting for the
     * SSL handshake to complete.
     */
    public static final int SSL_HANDSHAKE = 11;
    /**
     * This state corresponds to a resource load that is blocked waiting to
     * completely upload a request to a server. In the case of a HTTP POST
     * request, this state includes the period of time during which the message
     * body is being uploaded.
     */
    public static final int SENDING_REQUEST = 12;
    /**
     * This state corresponds to a resource load that is blocked waiting for the
     * response to a network request. In the case of a HTTP transaction, this
     * corresponds to the period after the request is sent and before all of the
     * response headers have been received.
     */
    public static final int WAITING_FOR_RESPONSE = 13;
    /**
     * This state corresponds to a resource load that is blocked waiting for a
     * read to complete. In the case of a HTTP transaction, this corresponds to
     * the period after the response headers have been received and before all
     * of the response body has been downloaded. (NOTE: This state only applies
     * for an {@link UrlRequest} while there is an outstanding
     * {@link UrlRequest#read} operation.)
     */
    public static final int READING_RESPONSE = 14;

    /**
     * Convert a {@link LoadState} static int to one of values listed above.
     */
    static int convertLoadState(int loadState) {
        assert loadState >= LoadState.IDLE && loadState <= LoadState.READING_RESPONSE;
        switch (loadState) {
            case (LoadState.IDLE):
                return IDLE;

            case (LoadState.WAITING_FOR_STALLED_SOCKET_POOL):
                return WAITING_FOR_STALLED_SOCKET_POOL;

            case (LoadState.WAITING_FOR_AVAILABLE_SOCKET):
                return WAITING_FOR_AVAILABLE_SOCKET;

            case (LoadState.WAITING_FOR_DELEGATE):
                return WAITING_FOR_DELEGATE;

            case (LoadState.WAITING_FOR_CACHE):
                return WAITING_FOR_CACHE;

            case (LoadState.DOWNLOADING_PROXY_SCRIPT):
                return DOWNLOADING_PROXY_SCRIPT;

            case (LoadState.RESOLVING_PROXY_FOR_URL):
                return RESOLVING_PROXY_FOR_URL;

            case (LoadState.RESOLVING_HOST_IN_PROXY_SCRIPT):
                return RESOLVING_HOST_IN_PROXY_SCRIPT;

            case (LoadState.ESTABLISHING_PROXY_TUNNEL):
                return ESTABLISHING_PROXY_TUNNEL;

            case (LoadState.RESOLVING_HOST):
                return RESOLVING_HOST;

            case (LoadState.CONNECTING):
                return CONNECTING;

            case (LoadState.SSL_HANDSHAKE):
                return SSL_HANDSHAKE;

            case (LoadState.SENDING_REQUEST):
                return SENDING_REQUEST;

            case (LoadState.WAITING_FOR_RESPONSE):
                return WAITING_FOR_RESPONSE;

            case (LoadState.READING_RESPONSE):
                return READING_RESPONSE;

            default:
                // A load state is retrieved but there is no corresponding
                // request status. This most likely means that the mapping is
                // incorrect.
                throw new IllegalArgumentException("No request status found.");
        }
    }
}
