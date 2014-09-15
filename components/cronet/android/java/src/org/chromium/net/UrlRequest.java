// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

/**
 * HTTP request (GET, PUT or POST).
 * Note:  All methods must be called on the Executor passed in during creation.
 */
public interface UrlRequest {
    /**
     * More setters go here.  They may only be called before start (Maybe
     * also allow during redirects).  Could optionally instead use arguments
     * to URLRequestFactory when creating the request.
     */

    /**
     * Sets the HTTP method verb to use for this request.
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
     * Starts the request, all callbacks go to listener.
     * @param listener
     */
    public void start(UrlRequestListener listener);

    /**
     * Can be called at any time.
     */
    public void cancel();

    /**
     *
     * @return True if the request has been cancelled by the embedder.
     * TBD(mmenke): False in all other cases (Including errors).
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
     * Note:  There are deliberately no accessors for the results of the request
     * here.  Having none removes any ambiguity over when they are populated,
     * particularly in the redirect case.
     */
}

