// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import java.nio.ByteBuffer;

/**
 * Users of Cronet extend this class to receive callbacks indicating the
 * progress of a {@link UrlRequest} being processed. An instance of this class
 * is passed in to {@link UrlRequest.Builder#UrlRequest.Builder UrlRequest.Builder()}
 * when constructing the {@code UrlRequest}.
 * <p>
 * Note:  All methods will be called on the thread of the
 * {@link java.util.concurrent.Executor} used during construction of the
 * {@code UrlRequest}.
 */
public abstract class UrlRequestListener {
    /**
     * Called whenever a redirect is encountered. This will only be called
     * between the call to {@link UrlRequest#start} and
     * {@link UrlRequestListener#onResponseStarted
     * UrlRequestListener.onResponseStarted()}. The body of the redirect
     * response, if it has one, will be ignored.
     *
     * The redirect will not be followed until the URLRequest's
     * {@link UrlRequest#followRedirect} method is called, either synchronously
     * or asynchronously.
     *
     * @param request Request being redirected.
     * @param info Response information.
     * @param newLocationUrl Location where request is redirected.
     */
    public abstract void onReceivedRedirect(
            UrlRequest request, UrlResponseInfo info, String newLocationUrl);

    /**
     * Called when the final set of headers, after all redirects, is received.
     * Will only be called once for each request.
     *
     * No other UrlRequestListener method will be called for the request,
     * including {@link UrlRequestListener#onSucceeded onSucceeded()} and {@link
     * UrlRequestListener#onFailed onFailed()}, until {@link UrlRequest#read
     * UrlRequest.read()} is called to attempt to start reading the response
     * body.
     *
     * @param request Request that started to get response.
     * @param info Response information.
     */
    public abstract void onResponseStarted(UrlRequest request, UrlResponseInfo info);

    /**
     * Called whenever part of the response body has been read. Only part of
     * the buffer may be populated, even if the entire response body has not yet
     * been consumed.
     *
     * No other UrlRequestListener method will be called for the request,
     * including {@link UrlRequestListener#onSucceeded onSucceeded()} and {@link
     * UrlRequestListener#onFailed onFailed()}, until {@link
     * UrlRequest#read UrlRequest.read()} is called to attempt to continue
     * reading the response body.
     *
     * @param request Request that received data.
     * @param info Response information.
     * @param byteBuffer The buffer that was passed in to
     *         {@link UrlRequest#read}, now containing the received data. The
     *         buffer's position is updated to the end of the received data. The
     *         buffer's limit is not changed.
     */
    public abstract void onReadCompleted(
            UrlRequest request, UrlResponseInfo info, ByteBuffer byteBuffer);

    /**
     * Called when request is completed successfully. Once called, no other
     * {@link UrlRequestListener} methods will be called.
     *
     * @param request Request that succeeded.
     * @param info Response information.
     */
    public abstract void onSucceeded(UrlRequest request, UrlResponseInfo info);

    /**
     * Called if request failed for any reason after {@link UrlRequest#start}.
     * Once called, no other {@link UrlRequestListener} methods will be called.
     * {@code error} provides information about the failure.
     *
     * @param request Request that failed.
     * @param info Response information. May be {@code null} if no response was
     *         received.
     * @param error information about error.
     */
    public abstract void onFailed(
            UrlRequest request, UrlResponseInfo info, UrlRequestException error);

    /**
     * Called if request was canceled via {@link UrlRequest#cancel}. Once
     * called, no other {@link UrlRequestListener} methods will be called.
     * Default implementation takes no action.
     *
     * @param request Request that was canceled.
     * @param info Response information. May be {@code null} if no response was
     *         received.
     */
    public void onCanceled(UrlRequest request, UrlResponseInfo info) {}
}
