// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import java.nio.ByteBuffer;
import java.util.concurrent.Executor;

/**
 * Controls an HTTP request (GET, PUT, POST etc).
 * Created using {@link UrlRequestContext#createRequest UrlRequestContext.createRequest()}.
 * Note: All methods must be called on the {@link Executor} passed in during creation.
 */
public interface UrlRequest {
    /**
     * Request status values returned by {@link #getStatus}.
     */
    public static class Status {
        /**
         * This state indicates that the request is completed, canceled, or is not
         * started.
         */
        public static final int INVALID = -1;
        /**
         * This state corresponds to a resource load that has either not yet begun
         * or is idle waiting for the consumer to do something to move things along
         * (e.g. when the consumer of a {@link UrlRequest} has not called
         * {@link #read} yet).
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

        private Status() {}

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

    /**
     * Listener class used with {@link #getStatus} to receive the status of a
     * {@link UrlRequest}.
     */
    public abstract class StatusListener {
        /**
         * Called on {@link UrlRequest}'s {@link Executor}'s thread when request
         * status is obtained.
         * @param status integer representing the status of the request. It is
         *         one of the values defined in {@link Status}.
         */
        public abstract void onStatus(int status);
    }

    /**
     * Lowest request priority. Passed to {@link UrlRequestContext#createRequest(String,
     * UrlRequestListener, Executor, int) UrlRequestContext.createRequest()}.
     */
    public static final int REQUEST_PRIORITY_IDLE = 0;
    /**
     * Very low request priority. Passed to {@link UrlRequestContext#createRequest(String,
     * UrlRequestListener, Executor, int) UrlRequestContext.createRequest()}.
     */
    public static final int REQUEST_PRIORITY_LOWEST = 1;
    /**
     * Low request priority. Passed to {@link UrlRequestContext#createRequest(String,
     * UrlRequestListener, Executor, int) UrlRequestContext.createRequest()}.
     */
    public static final int REQUEST_PRIORITY_LOW = 2;
    /**
     * Medium request priority. Passed to {@link UrlRequestContext#createRequest(String,
     * UrlRequestListener, Executor, int) UrlRequestContext.createRequest()}.
     */
    public static final int REQUEST_PRIORITY_MEDIUM = 3;
    /**
     * Highest request priority. Passed to {@link UrlRequestContext#createRequest(String,
     * UrlRequestListener, Executor, int) UrlRequestContext.createRequest()}.
     */
    public static final int REQUEST_PRIORITY_HIGHEST = 4;

    // More setters go here. They may only be called before start (Maybe
    // also allow during redirects). Could optionally instead use arguments
    // to URLRequestFactory when creating the request.

    /**
     * Sets the HTTP method verb to use for this request. Must be done before
     * request has started.
     *
     * <p>The default when this method is not called is "GET" if the request has
     * no body or "POST" if it does.
     *
     * @param method "GET", "HEAD", "DELETE", "POST" or "PUT".
     */
    public void setHttpMethod(String method);

    /**
     * Adds a request header. Must be done before request has started.
     *
     * @param header header name.
     * @param value header value.
     */
    public void addHeader(String header, String value);

    /**
     * Sets upload data provider. Must be done before request has started. May only be
     * invoked once per request. Switches method to "POST" if not explicitly
     * set. Starting the request will throw an exception if a Content-Type
     * header is not set.
     *
     * @param uploadDataProvider responsible for providing the upload data.
     * @param executor All {@code uploadDataProvider} methods will be called
     *     using this {@code Executor}. May optionally be the same
     *     {@code Executor} the request itself is using.
     */
    public void setUploadDataProvider(UploadDataProvider uploadDataProvider, Executor executor);

    /**
     * Starts the request, all callbacks go to listener. May only be called
     * once. May not be called if {@link #cancel} has been called.
     */
    public void start();

    /**
     * Follows a pending redirect. Must only be called at most once for each
     * invocation of {@link UrlRequestListener#onReceivedRedirect
     * UrlRequestListener.onReceivedRedirect()}.
     */
    public void followRedirect();

    /**
     * Attempts to read part of the response body into the provided buffer.
     * Must only be called at most once in response to each invocation of the
     * {@link UrlRequestListener#onResponseStarted onResponseStarted} and {@link
     * UrlRequestListener#onReadCompleted onReadCompleted} methods of the {@link
     * UrlRequestListener}. Each call will result in an asynchronous call to
     * either the {@link UrlRequestListener UrlRequestListener's}
     * {@link UrlRequestListener#onReadCompleted onReadCompleted} method if data
     * is read, its {@link UrlRequestListener#onSucceeded onSucceeded} method if
     * there's no more data to read, or its {@link UrlRequestListener#onFailed
     * onFailed} method if there's an error.
     *
     * @param buffer {@link ByteBuffer} to write response body to. Must be a
     *     direct ByteBuffer. The embedder must not read or modify buffer's
     *     position, limit, or data between its position and capacity until the
     *     request calls back into the {@link URLRequestListener}. If the
     *     request is cancelled before such a call occurs, it's never safe to
     *     use the buffer again.
     * @deprecated Use readNew() instead though note that it updates the
     *     buffer's position not limit.
     */
    // TODO(mmenke):  Should we add some ugliness to allow reclaiming the buffer
    //     on cancellation?  If it's a C++-allocated buffer, then the consumer
    //     can never safely free it, unless they put off cancelling a request
    //     until a callback has been invoked.
    // TODO(pauljensen): Switch all callers to call readNew().
    @Deprecated public void read(ByteBuffer buffer);

    /**
     * Attempts to read part of the response body into the provided buffer.
     * Must only be called at most once in response to each invocation of the
     * {@link UrlRequestListener#onResponseStarted onResponseStarted} and {@link
     * UrlRequestListener#onReadCompleted onReadCompleted} methods of the {@link
     * UrlRequestListener}. Each call will result in an asynchronous call to
     * either the {@link UrlRequestListener UrlRequestListener's}
     * {@link UrlRequestListener#onReadCompleted onReadCompleted} method if data
     * is read, its {@link UrlRequestListener#onSucceeded onSucceeded} method if
     * there's no more data to read, or its {@link UrlRequestListener#onFailed
     * onFailed} method if there's an error.
     *
     * @param buffer {@link ByteBuffer} to write response body to. Must be a
     *     direct ByteBuffer. The embedder must not read or modify buffer's
     *     position, limit, or data between its position and limit until the
     *     request calls back into the {@link URLRequestListener}. If the
     *     request is cancelled before such a call occurs, it's never safe to
     *     use the buffer again.
     */
    // TODO(mmenke):  Should we add some ugliness to allow reclaiming the buffer
    //     on cancellation?  If it's a C++-allocated buffer, then the consumer
    //     can never safely free it, unless they put off cancelling a request
    //     until a callback has been invoked.
    // TODO(pauljensen): Rename to read() once original read() is removed.
    public void readNew(ByteBuffer buffer);

    /**
     * Cancels the request.
     *
     * Can be called at any time. If the {@link Executor} passed in during
     * {@code UrlRequest} construction runs tasks on a single thread, and cancel
     * is called on that thread, no listener methods will be invoked after
     * cancel is called. Otherwise, at most one listener method may be made
     * after cancel has completed.
     */
    public void cancel();

    /**
     * Returns {@code true} if the request was successfully started and is now
     * done (completed, canceled, or failed).
     * @return {@code true} if the request was successfully started and is now
     *         done (completed, canceled, or failed).
     */
    public boolean isDone();

    /**
     * Disables cache for the request. If context is not set up to use cache,
     * this call has no effect.
     */
    public void disableCache();

    /**
     * Queries the status of the request.
     * @param listener a {@link StatusListener} that will be called back with
     *         the request's current status. {@code listener} will be called
     *         back on the {@link Executor} passed in when the request was
     *         created.
     */
    public void getStatus(final StatusListener listener);

    // Note:  There are deliberately no accessors for the results of the request
    // here. Having none removes any ambiguity over when they are populated,
    // particularly in the redirect case.
}

