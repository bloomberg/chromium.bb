// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import java.net.URL;
import java.nio.ByteBuffer;

/**
 * Note:  All methods will be called on the thread of the Executor used during
 * construction of the AsyncUrlRequest.
 */
public abstract interface AsyncUrlRequestListener {
    /**
     * Called before following redirects.  The redirect will automatically be
     * followed, unless the request is paused or cancelled during this
     * callback.  If the redirect response has a body, it will be ignored.
     * This will only be called between start and onResponseStarted.
     *
     * @param request Request being redirected.
     * @param info Response information.
     * @param new_location Location where request is redirected.
     */
    public void onRedirect(AsyncUrlRequest request, ResponseInfo info,
                           URL new_location);

    /**
     * Called when the final set of headers, after all redirects,
     * is received. Can only be called once for each request.
     *
     * @param request Request that started to get response.
     * @param info Response information.
     */
    public void onResponseStarted(AsyncUrlRequest request, ResponseInfo info);

    /**
     * Called whenever data is received.  The ByteBuffer remains
     * valid only for the duration of the callback.  Or if the callback
     * pauses the request, it remains valid until the request is resumed.
     * Cancelling the request also invalidates the buffer.
     *
     * @param request Request that received data.
     * @param byteBuffer Received data.
     */
    public void onDataReceived(AsyncUrlRequest request, ByteBuffer byteBuffer);

    /**
     * Called when request is complete, no callbacks will be called afterwards.
     *
     * @param request Request that is complete.
     */
    public void onComplete(AsyncUrlRequest request);

    /**
     * Can be called at any point between start() and onComplete().  Once
     * called, no other functions can be called.  AsyncUrlRequestException
     * provides information about error.
     *
     * @param request Request that received an error.
     * @param error information about error.
     */
    public void onError(AsyncUrlRequest request,
                        AsyncUrlRequestException error);
}
