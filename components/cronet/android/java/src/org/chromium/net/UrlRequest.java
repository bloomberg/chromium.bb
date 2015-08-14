// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import java.nio.ByteBuffer;
import java.util.concurrent.Executor;

/**
 * Controls an HTTP request (GET, PUT, POST etc).
 * Created using {@link UrlRequestContext#createRequest UrlRequestContext.createRequest()}.
 * Note:  All methods must be called on the {@link Executor} passed in during creation.
 */
public interface UrlRequest {
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
     */
    // TODO(mmenke):  Should we add some ugliness to allow reclaiming the buffer
    //     on cancellation?  If it's a C++-allocated buffer, then the consumer
    //     can never safely free it, unless they put off cancelling a request
    //     until a callback has been invoked.
    public void read(ByteBuffer buffer);

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

