// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.util.Log;

import org.apache.http.conn.ConnectTimeoutException;
import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.nio.channels.ReadableByteChannel;
import java.nio.channels.WritableByteChannel;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

/**
 * Network request using the native http stack implementation.
 */
@JNINamespace("cronet")
public class ChromiumUrlRequest implements HttpUrlRequest {
    /**
     * Native adapter object, owned by UrlRequest.
     */
    private long mUrlRequestAdapter;
    private final ChromiumUrlRequestContext mRequestContext;
    private final String mUrl;
    private final int mPriority;
    private final Map<String, String> mHeaders;
    private final WritableByteChannel mSink;
    private Map<String, String> mAdditionalHeaders;
    private String mUploadContentType;
    private String mMethod;
    private byte[] mUploadData;
    private ReadableByteChannel mUploadChannel;
    private boolean mChunkedUpload;
    private IOException mSinkException;
    private volatile boolean mStarted;
    private volatile boolean mCanceled;
    private volatile boolean mRecycled;
    private volatile boolean mFinished;
    private boolean mHeadersAvailable;
    private String mContentType;
    private long mUploadContentLength;
    private final HttpUrlRequestListener mListener;
    private boolean mBufferFullResponse;
    private long mOffset;
    private long mContentLength;
    private long mContentLengthLimit;
    private boolean mCancelIfContentLengthOverLimit;
    private boolean mContentLengthOverLimit;
    private boolean mSkippingToOffset;
    private long mSize;
    private final Object mLock = new Object();

    public ChromiumUrlRequest(ChromiumUrlRequestContext requestContext,
            String url, int priority, Map<String, String> headers,
            HttpUrlRequestListener listener) {
        this(requestContext, url, priority, headers,
                new ChunkedWritableByteChannel(), listener);
        mBufferFullResponse = true;
    }

    /**
     * Constructor.
     *
     * @param requestContext The context.
     * @param url The URL.
     * @param priority Request priority, e.g. {@link #REQUEST_PRIORITY_MEDIUM}.
     * @param headers HTTP headers.
     * @param sink The output channel into which downloaded content will be
     *            written.
     */
    public ChromiumUrlRequest(ChromiumUrlRequestContext requestContext,
            String url, int priority, Map<String, String> headers,
            WritableByteChannel sink, HttpUrlRequestListener listener) {
        if (requestContext == null) {
            throw new NullPointerException("Context is required");
        }
        if (url == null) {
            throw new NullPointerException("URL is required");
        }
        mRequestContext = requestContext;
        mUrl = url;
        mPriority = convertRequestPriority(priority);
        mHeaders = headers;
        mSink = sink;
        mUrlRequestAdapter = nativeCreateRequestAdapter(
                mRequestContext.getChromiumUrlRequestContextAdapter(),
                mUrl,
                mPriority);
        mListener = listener;
    }

    @Override
    public void setOffset(long offset) {
        mOffset = offset;
        if (offset != 0) {
            addHeader("Range", "bytes=" + offset + "-");
        }
    }

    /**
     * The compressed content length as reported by the server.  May be -1 if
     * the server did not provide a length.  Some servers may also report the
     * wrong number.  Since this is the compressed content length, and only
     * uncompressed content is returned by the consumer, the consumer should
     * not rely on this value.
     */
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
    public int getHttpStatusCode() {
        int httpStatusCode = nativeGetHttpStatusCode(mUrlRequestAdapter);

        // TODO(mef): Investigate the following:
        // If we have been able to successfully resume a previously interrupted
        // download, the status code will be 206, not 200. Since the rest of the
        // application is expecting 200 to indicate success, we need to fake it.
        if (httpStatusCode == 206) {
            httpStatusCode = 200;
        }
        return httpStatusCode;
    }

    /**
     * Returns an exception if any, or null if the request was completed
     * successfully.
     */
    @Override
    public IOException getException() {
        if (mSinkException != null) {
            return mSinkException;
        }

        validateNotRecycled();

        int errorCode = nativeGetErrorCode(mUrlRequestAdapter);
        switch (errorCode) {
            case ChromiumUrlRequestError.SUCCESS:
                if (mContentLengthOverLimit) {
                    return new ResponseTooLargeException();
                }
                return null;
            case ChromiumUrlRequestError.UNKNOWN:
                return new IOException(
                        nativeGetErrorString(mUrlRequestAdapter));
            case ChromiumUrlRequestError.MALFORMED_URL:
                return new MalformedURLException("Malformed URL: " + mUrl);
            case ChromiumUrlRequestError.CONNECTION_TIMED_OUT:
                return new ConnectTimeoutException("Connection timed out");
            case ChromiumUrlRequestError.UNKNOWN_HOST:
                String host;
                try {
                    host = new URL(mUrl).getHost();
                } catch (MalformedURLException e) {
                    host = mUrl;
                }
                return new UnknownHostException("Unknown host: " + host);
            default:
                throw new IllegalStateException(
                        "Unrecognized error code: " + errorCode);
        }
    }

    @Override
    public ByteBuffer getByteBuffer() {
        return ((ChunkedWritableByteChannel)getSink()).getByteBuffer();
    }

    @Override
    public byte[] getResponseAsBytes() {
        return ((ChunkedWritableByteChannel)getSink()).getBytes();
    }

    /**
     * Adds a request header. Must be done before request has started.
     */
    public void addHeader(String header, String value) {
        synchronized (mLock) {
            validateNotStarted();
            if (mAdditionalHeaders == null) {
                mAdditionalHeaders = new HashMap<String, String>();
            }
            mAdditionalHeaders.put(header, value);
        }
    }

    /**
     * Sets data to upload as part of a POST or PUT request.
     *
     * @param contentType MIME type of the upload content or null if this is not
     *            an upload.
     * @param data The content that needs to be uploaded.
     */
    public void setUploadData(String contentType, byte[] data) {
        synchronized (mLock) {
            validateNotStarted();
            mUploadContentType = contentType;
            mUploadData = data;
            mUploadChannel = null;
            mChunkedUpload = false;
        }
    }

    /**
     * Sets a readable byte channel to upload as part of a POST or PUT request.
     *
     * @param contentType MIME type of the upload content or null if this is not
     *            an upload request.
     * @param channel The channel to read to read upload data from if this is an
     *            upload request.
     * @param contentLength The length of data to upload.
     */
    public void setUploadChannel(String contentType,
            ReadableByteChannel channel, long contentLength) {
        synchronized (mLock) {
            validateNotStarted();
            mUploadContentType = contentType;
            mUploadChannel = channel;
            mUploadContentLength = contentLength;
            mUploadData = null;
            mChunkedUpload = false;
        }
    }

    /**
     * Sets this request up for chunked uploading. To upload data call
     * {@link #appendChunk(ByteBuffer, boolean)} after {@link #start()}.
     *
     * @param contentType MIME type of the post content or null if this is not a
     *            POST request.
     */
    public void setChunkedUpload(String contentType) {
        synchronized (mLock) {
            validateNotStarted();
            mUploadContentType = contentType;
            mChunkedUpload = true;
            mUploadData = null;
            mUploadChannel = null;
        }
    }

    /**
     * Uploads a new chunk. Must have called {@link #setChunkedUpload(String)}
     * and {@link #start()}.
     *
     * @param chunk The data, which will not be modified. It must not be empty
     *            and its current position must be zero.
     * @param isLastChunk Whether this chunk is the last one.
     */
    public void appendChunk(ByteBuffer chunk, boolean isLastChunk)
            throws IOException {
        if (!chunk.hasRemaining()) {
            throw new IllegalArgumentException(
                    "Attempted to write empty buffer.");
        }
        if (chunk.position() != 0) {
            throw new IllegalArgumentException("The position must be zero.");
        }
        synchronized (mLock) {
            if (!mStarted) {
                throw new IllegalStateException("Request not yet started.");
            }
            if (!mChunkedUpload) {
                throw new IllegalStateException(
                        "Request not set for chunked uploadind.");
            }
            if (mUrlRequestAdapter == 0) {
                throw new IOException("Native peer destroyed.");
            }
            nativeAppendChunk(mUrlRequestAdapter, chunk, chunk.limit(),
                    isLastChunk);
        }
    }

    /**
     * Sets HTTP method for upload request. Only PUT or POST are allowed.
     */
    public void setHttpMethod(String method) {
        validateNotStarted();
        if (!("PUT".equals(method) || "POST".equals(method))) {
            throw new IllegalArgumentException("Only PUT or POST are allowed.");
        }
        mMethod = method;
    }

    public WritableByteChannel getSink() {
        return mSink;
    }

    public void start() {
        synchronized (mLock) {
            if (mCanceled) {
                return;
            }

            validateNotStarted();
            validateNotRecycled();

            mStarted = true;

            String method = mMethod;
            if (method == null &&
                    ((mUploadData != null && mUploadData.length > 0) ||
                      mUploadChannel != null || mChunkedUpload)) {
                // Default to POST if there is data to upload but no method was
                // specified.
                method = "POST";
            }

            if (method != null) {
                nativeSetMethod(mUrlRequestAdapter, method);
            }

            if (mHeaders != null && !mHeaders.isEmpty()) {
                for (Entry<String, String> entry : mHeaders.entrySet()) {
                    nativeAddHeader(mUrlRequestAdapter, entry.getKey(),
                            entry.getValue());
                }
            }

            if (mAdditionalHeaders != null) {
                for (Entry<String, String> entry :
                        mAdditionalHeaders.entrySet()) {
                    nativeAddHeader(mUrlRequestAdapter, entry.getKey(),
                            entry.getValue());
                }
            }

            if (mUploadData != null && mUploadData.length > 0) {
                nativeSetUploadData(mUrlRequestAdapter, mUploadContentType,
                                    mUploadData);
            } else if (mUploadChannel != null) {
                nativeSetUploadChannel(mUrlRequestAdapter, mUploadContentType,
                                       mUploadContentLength);
            } else if (mChunkedUpload) {
                nativeEnableChunkedUpload(mUrlRequestAdapter,
                                          mUploadContentType);
            }

            nativeStart(mUrlRequestAdapter);
          }
    }

    public void cancel() {
        synchronized (mLock) {
            if (mCanceled) {
                return;
            }

            mCanceled = true;

            if (!mRecycled) {
                nativeCancel(mUrlRequestAdapter);
            }
        }
    }

    public boolean isCanceled() {
        synchronized (mLock) {
            return mCanceled;
        }
    }

    public boolean isRecycled() {
        synchronized (mLock) {
            return mRecycled;
        }
    }

    public String getContentType() {
        return mContentType;
    }

    public String getHeader(String name) {
        validateHeadersAvailable();
        return nativeGetHeader(mUrlRequestAdapter, name);
    }

    // All response headers.
    public Map<String, List<String>> getAllHeaders() {
        validateHeadersAvailable();
        ResponseHeadersMap result = new ResponseHeadersMap();
        nativeGetAllHeaders(mUrlRequestAdapter, result);
        return result;
    }

    public String getUrl() {
        return mUrl;
    }

    private static int convertRequestPriority(int priority) {
        switch (priority) {
            case HttpUrlRequest.REQUEST_PRIORITY_IDLE:
                return ChromiumUrlRequestPriority.IDLE;
            case HttpUrlRequest.REQUEST_PRIORITY_LOWEST:
                return ChromiumUrlRequestPriority.LOWEST;
            case HttpUrlRequest.REQUEST_PRIORITY_LOW:
                return ChromiumUrlRequestPriority.LOW;
            case HttpUrlRequest.REQUEST_PRIORITY_MEDIUM:
                return ChromiumUrlRequestPriority.MEDIUM;
            case HttpUrlRequest.REQUEST_PRIORITY_HIGHEST:
                return ChromiumUrlRequestPriority.HIGHEST;
            default:
                return ChromiumUrlRequestPriority.MEDIUM;
        }
    }

    private void onContentLengthOverLimit() {
        mContentLengthOverLimit = true;
        cancel();
    }

    /**
     * A callback invoked when the response has been fully consumed.
     */
    private void onRequestComplete() {
        mListener.onRequestComplete(this);
    }

    private void validateNotRecycled() {
        if (mRecycled) {
            throw new IllegalStateException("Accessing recycled request");
        }
    }

    private void validateNotStarted() {
        if (mStarted) {
            throw new IllegalStateException("Request already started");
        }
    }

    private void validateHeadersAvailable() {
        if (!mHeadersAvailable) {
            throw new IllegalStateException("Response headers not available");
        }
    }

    // Private methods called by native library.

    /**
     * If @CalledByNative method throws an exception, request gets cancelled
     * and exception could be retrieved from request using getException().
     */
    private void onCalledByNativeException(Exception e) {
        mSinkException = new IOException(
                "CalledByNative method has thrown an exception", e);
        Log.e(ChromiumUrlRequestContext.LOG_TAG,
                "Exception in CalledByNative method", e);
        try {
            cancel();
        } catch (Exception cancel_exception) {
            Log.e(ChromiumUrlRequestContext.LOG_TAG,
                    "Exception trying to cancel request", cancel_exception);
        }
    }

    /**
     * A callback invoked when the first chunk of the response has arrived.
     */
    @CalledByNative
    private void onResponseStarted() {
        try {
            mContentType = nativeGetContentType(mUrlRequestAdapter);
            mContentLength = nativeGetContentLength(mUrlRequestAdapter);
            mHeadersAvailable = true;

            if (mContentLengthLimit > 0 &&
                    mContentLength > mContentLengthLimit &&
                    mCancelIfContentLengthOverLimit) {
                onContentLengthOverLimit();
                    return;
            }

            if (mBufferFullResponse && mContentLength != -1 &&
                    !mContentLengthOverLimit) {
                ((ChunkedWritableByteChannel)getSink()).setCapacity(
                        (int)mContentLength);
            }

            if (mOffset != 0) {
                // The server may ignore the request for a byte range, in which
                // case status code will be 200, instead of 206. Note that we
                // cannot call getHttpStatusCode as it rewrites 206 into 200.
                if (nativeGetHttpStatusCode(mUrlRequestAdapter) == 200) {
                    // TODO(mef): Revisit this logic.
                    if (mContentLength != -1) {
                        mContentLength -= mOffset;
                    }
                    mSkippingToOffset = true;
                } else {
                    mSize = mOffset;
                }
            }
            mListener.onResponseStarted(this);
        } catch (Exception e) {
            onCalledByNativeException(e);
        }
    }

    /**
     * Consumes a portion of the response.
     *
     * @param byteBuffer The ByteBuffer to append. Must be a direct buffer, and
     *            no references to it may be retained after the method ends, as
     *            it wraps code managed on the native heap.
     */
    @CalledByNative
    private void onBytesRead(ByteBuffer buffer) {
        try {
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

            boolean contentLengthOverLimit =
                    (mContentLengthLimit != 0 && mSize > mContentLengthLimit);
            if (contentLengthOverLimit) {
                buffer.limit(size - (int)(mSize - mContentLengthLimit));
            }

            while (buffer.hasRemaining()) {
                mSink.write(buffer);
            }
            if (contentLengthOverLimit) {
                onContentLengthOverLimit();
            }
        } catch (Exception e) {
            onCalledByNativeException(e);
        }
    }

    /**
     * Notifies the listener, releases native data structures.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void finish() {
        try {
            synchronized (mLock) {
                mFinished = true;

                if (mRecycled) {
                    return;
                }
                try {
                    mSink.close();
                } catch (IOException e) {
                    // Ignore
                }
                try {
                    if (mUploadChannel != null && mUploadChannel.isOpen()) {
                        mUploadChannel.close();
                    }
                } catch (IOException e) {
                    // Ignore
                }
                onRequestComplete();
                nativeDestroyRequestAdapter(mUrlRequestAdapter);
                mUrlRequestAdapter = 0;
                mRecycled = true;
            }
        } catch (Exception e) {
            mSinkException = new IOException("Exception in finish", e);
        }
    }

    /**
     * Appends header |name| with value |value| to |headersMap|.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onAppendResponseHeader(ResponseHeadersMap headersMap,
            String name, String value) {
        try {
            if (!headersMap.containsKey(name)) {
                headersMap.put(name, new ArrayList<String>());
            }
            headersMap.get(name).add(value);
        } catch (Exception e) {
            onCalledByNativeException(e);
        }
    }

    /**
     * Reads a sequence of bytes from upload channel into the given buffer.
     * @param dest The buffer into which bytes are to be transferred.
     * @return Returns number of bytes read (could be 0) or -1 and closes
     * the channel if error occured.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private int readFromUploadChannel(ByteBuffer dest) {
        try {
            if (mUploadChannel == null || !mUploadChannel.isOpen())
                return -1;
            int result = mUploadChannel.read(dest);
            if (result < 0) {
                mUploadChannel.close();
                return 0;
            }
            return result;
        } catch (Exception e) {
            onCalledByNativeException(e);
        }
        return -1;
    }

    // Native methods are implemented in chromium_url_request.cc.

    private native long nativeCreateRequestAdapter(
            long ChromiumUrlRequestContextAdapter, String url, int priority);

    private native void nativeAddHeader(long urlRequestAdapter, String name,
            String value);

    private native void nativeSetMethod(long urlRequestAdapter, String method);

    private native void nativeSetUploadData(long urlRequestAdapter,
            String contentType, byte[] content);

    private native void nativeSetUploadChannel(long urlRequestAdapter,
            String contentType, long contentLength);

    private native void nativeEnableChunkedUpload(long urlRequestAdapter,
            String contentType);

    private native void nativeAppendChunk(long urlRequestAdapter,
            ByteBuffer chunk, int chunkSize, boolean isLastChunk);

    private native void nativeStart(long urlRequestAdapter);

    private native void nativeCancel(long urlRequestAdapter);

    private native void nativeDestroyRequestAdapter(long urlRequestAdapter);

    private native int nativeGetErrorCode(long urlRequestAdapter);

    private native int nativeGetHttpStatusCode(long urlRequestAdapter);

    private native String nativeGetErrorString(long urlRequestAdapter);

    private native String nativeGetContentType(long urlRequestAdapter);

    private native long nativeGetContentLength(long urlRequestAdapter);

    private native String nativeGetHeader(long urlRequestAdapter, String name);

    private native void nativeGetAllHeaders(long urlRequestAdapter,
            ResponseHeadersMap headers);

    // Explicit class to work around JNI-generator generics confusion.
    private class ResponseHeadersMap extends HashMap<String, List<String>> {
    }
}
