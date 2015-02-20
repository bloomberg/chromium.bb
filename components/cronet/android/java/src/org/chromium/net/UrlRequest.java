// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import java.util.concurrent.Executor;

/**
 * HTTP request (GET, PUT or POST).
 * Note:  All methods must be called on the Executor passed in during creation.
 */
public interface UrlRequest {
    public static final int REQUEST_PRIORITY_IDLE = 0;

    public static final int REQUEST_PRIORITY_LOWEST = 1;

    public static final int REQUEST_PRIORITY_LOW = 2;

    public static final int REQUEST_PRIORITY_MEDIUM = 3;

    public static final int REQUEST_PRIORITY_HIGHEST = 4;

    /**
     * More setters go here.  They may only be called before start (Maybe
     * also allow during redirects).  Could optionally instead use arguments
     * to URLRequestFactory when creating the request.
     */

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
     * @param header Header name
     * @param value Header value
     */
    public void addHeader(String header, String value);

    /**
     * Sets upload data. Must be done before request has started. May only be
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
     * once. May not be called if cancel has been called on the request.
     */
    public void start();

    /**
     * Cancels the request.
     *
     * Can be called at any time.  If the Executor passed to UrlRequest on
     * construction runs tasks on a single thread, and cancel is called on that
     * thread, no listener methods will be invoked after cancel is called.
     * Otherwise, at most one listener method may be made after cancel has
     * completed.
     */
    public void cancel();

    /**
     * @return True if the request has been cancelled by the embedder.
     * False in all other cases (Including errors).
     */
    public boolean isCanceled();

    /**
     * Can be called at any time, but the request may continue behind the
     * scenes, depending on when it's called.  None of the listener's methods
     * will be called while paused, until and unless the request is resumed.
     * (Note:  This allows us to have more than one ByteBuffer in flight,
     * if we want, as well as allow pausing at any point).
     *
     * TBD: May need different pause behavior.
     */
    public void pause();

    /**
     * Returns True if paused. False if not paused or is cancelled.
     * @return
     */
    public boolean isPaused();

    /**
     * When resuming, any pending callback to the listener will be called
     * asynchronously.
     */
    public void resume();

    /**
     * Disables cache for the request. If context is not set up to use cache,
     * this call has no effect.
     */
    public void disableCache();

    /**
     * Note:  There are deliberately no accessors for the results of the request
     * here.  Having none removes any ambiguity over when they are populated,
     * particularly in the redirect case.
     */
}

