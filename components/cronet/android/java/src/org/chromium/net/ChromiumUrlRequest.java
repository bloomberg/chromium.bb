// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.WritableByteChannel;
import java.util.Map;

/**
 * Network request using the native http stack implementation.
 */
public class ChromiumUrlRequest extends UrlRequest implements HttpUrlRequest {

    private final HttpUrlRequestListener mListener;

    private boolean mBufferFullResponse;

    private long mOffset;

    private long mContentLength;

    private long mContentLengthLimit;

    private boolean mCancelIfContentLengthOverLimit;

    private boolean mContentLengthOverLimit;

    private boolean mSkippingToOffset;

    private long mSize;

    public ChromiumUrlRequest(UrlRequestContext requestContext,
            String url, int priority, Map<String, String> headers,
            HttpUrlRequestListener listener) {
        this(requestContext, url, priority, headers,
                new ChunkedWritableByteChannel(), listener);
        mBufferFullResponse = true;
    }

    public ChromiumUrlRequest(UrlRequestContext requestContext,
            String url, int priority, Map<String, String> headers,
            WritableByteChannel sink, HttpUrlRequestListener listener) {
        super(requestContext, url, convertRequestPriority(priority), headers,
                sink);
        mListener = listener;
    }

    private static int convertRequestPriority(int priority) {
        switch (priority) {
            case HttpUrlRequest.REQUEST_PRIORITY_IDLE:
                return UrlRequestPriority.IDLE;
            case HttpUrlRequest.REQUEST_PRIORITY_LOWEST:
                return UrlRequestPriority.LOWEST;
            case HttpUrlRequest.REQUEST_PRIORITY_LOW:
                return UrlRequestPriority.LOW;
            case HttpUrlRequest.REQUEST_PRIORITY_MEDIUM:
                return UrlRequestPriority.MEDIUM;
            case HttpUrlRequest.REQUEST_PRIORITY_HIGHEST:
                return UrlRequestPriority.HIGHEST;
            default:
                return UrlRequestPriority.MEDIUM;
        }
    }

    @Override
    public void setOffset(long offset) {
        mOffset = offset;
        if (offset != 0) {
            addHeader("Range", "bytes=" + offset + "-");
        }
    }

    @Override
    public long getContentLength() {
        return mContentLength;
    }

    @Override
    public void setContentLengthLimit(long limit, boolean cancelEarly) {
        mContentLengthLimit = limit;
        mCancelIfContentLengthOverLimit = cancelEarly;
    }

    @Override
    protected void onResponseStarted() {
        super.onResponseStarted();

        mContentLength = super.getContentLength();
        if (mContentLengthLimit > 0 && mContentLength > mContentLengthLimit
                && mCancelIfContentLengthOverLimit) {
            onContentLengthOverLimit();
            return;
        }

        if (mBufferFullResponse && mContentLength != -1
                && !mContentLengthOverLimit) {
            ((ChunkedWritableByteChannel)getSink()).setCapacity(
                    (int)mContentLength);
        }

        if (mOffset != 0) {
            // The server may ignore the request for a byte range.
            if (super.getHttpStatusCode() == 200) {
                if (mContentLength != -1) {
                    mContentLength -= mOffset;
                }
                mSkippingToOffset = true;
            } else {
                mSize = mOffset;
            }
        }
        mListener.onResponseStarted(this);
    }

    @Override
    protected void onBytesRead(ByteBuffer buffer) {
        if (mContentLengthOverLimit) {
            return;
        }

        int size = buffer.remaining();
        mSize += size;
        if (mSkippingToOffset) {
            if (mSize <= mOffset) {
                return;
            } else {
                mSkippingToOffset = false;
                buffer.position((int)(mOffset - (mSize - size)));
            }
        }

        if (mContentLengthLimit != 0 && mSize > mContentLengthLimit) {
            buffer.limit(size - (int)(mSize - mContentLengthLimit));
            super.onBytesRead(buffer);
            onContentLengthOverLimit();
            return;
        }

        super.onBytesRead(buffer);
    }

    private void onContentLengthOverLimit() {
        mContentLengthOverLimit = true;
        cancel();
    }

    @Override
    protected void onRequestComplete() {
        mListener.onRequestComplete(this);
    }

    @Override
    public int getHttpStatusCode() {
        int httpStatusCode = super.getHttpStatusCode();

        // TODO(mef): Investigate the following:
        // If we have been able to successfully resume a previously interrupted
        // download,
        // the status code will be 206, not 200. Since the rest of the
        // application is
        // expecting 200 to indicate success, we need to fake it.
        if (httpStatusCode == 206) {
            httpStatusCode = 200;
        }
        return httpStatusCode;
    }

    @Override
    public IOException getException() {
        IOException ex = super.getException();
        if (ex == null && mContentLengthOverLimit) {
            ex = new ResponseTooLargeException();
        }
        return ex;
    }

    @Override
    public ByteBuffer getByteBuffer() {
        return ((ChunkedWritableByteChannel)getSink()).getByteBuffer();
    }

    @Override
    public byte[] getResponseAsBytes() {
        return ((ChunkedWritableByteChannel)getSink()).getBytes();
    }
}
