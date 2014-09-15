// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import java.net.URL;
import java.nio.ByteBuffer;

/**
 * Note:  All methods will be called on the thread of the Executor used during
 * construction of the UrlRequest.
 */
public interface UrlRequestListener {
    /**
     * Called before following redirects.  The redirect will automatically be
     * followed, unless the request is paused or cancelled during this
     * callback.  If the redirect response has a body, it will be ignored.
     * This will only be called between start and onResponseStarted.
     *
     * @param request Request being redirected.
     * @param info Response information.
     * @param newLocation Location where request is redirected.
     */
    public void onRedirect(UrlRequest request, ResponseInfo info,
                           URL newLocation);

    /**
     * Called when the final set of headers, after all redirects,
     * is received. Can only be called once for each request.
     *
     * @param request Request that started to get response.
     * @param info Response information.
     */
    public void onResponseStarted(UrlRequest request, ResponseInfo info);

    /**
     * Called whenever data is received.  The ByteBuffer remains
     * valid only for the duration of the callback.  Or if the callback
     * pauses the request, it remains valid until the request is resumed.
     * Cancelling the request also invalidates the buffer.
     *
     * @param request Request that received data.
     * @param byteBuffer Received data.
     */
    public void onDataReceived(UrlRequest request, ByteBuffer byteBuffer);

    /**
     * Called when request is complete, no callbacks will be called afterwards.
     *
     * @param request Request that is complete.
     */
    public void onComplete(UrlRequest request);

    /**
     * Can be called at any point between start() and onComplete().  Once
     * called, no other functions can be called.  UrlRequestException
     * provides information about error.
     *
     * @param request Request that received an error.
     * @param error information about error.
     */
    public void onError(UrlRequest request,
                        UrlRequestException error);
}
