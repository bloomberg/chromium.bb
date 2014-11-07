// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

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
     * @param newLocationUrl Location where request is redirected.
     */
    public void onRedirect(UrlRequest request,
            ResponseInfo info,
            String newLocationUrl);

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
     * @param info Response information.
     * @param byteBuffer Received data.
     */
    public void onDataReceived(UrlRequest request,
            ResponseInfo info,
            ByteBuffer byteBuffer);

    /**
     * Called when request is completed successfully, no callbacks will be
     * called afterwards.
     *
     * @param request Request that succeeded.
     * @param info Response information.
     */
    public void onSucceeded(UrlRequest request, ExtendedResponseInfo info);

    /**
     * Called if request failed for any reason after start().  Once
     * called, no other functions can be called.  UrlRequestException
     * provides information about error.
     *
     * @param request Request that failed.
     * @param info Response information.
     * @param error information about error.
     */
    public void onFailed(UrlRequest request,
            ResponseInfo info,
            UrlRequestException error);
}
