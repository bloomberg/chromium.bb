// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

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
public class UrlRequest {
    private static final class ContextLock {
    }

    private final UrlRequestContext mRequestContext;
    private final String mUrl;
    private final int mPriority;
    private final Map<String, String> mHeaders;
    private final WritableByteChannel mSink;
    private Map<String, String> mAdditionalHeaders;
    private String mUploadContentType;
    private String mMethod;
    private byte[] mUploadData;
    private ReadableByteChannel mUploadChannel;
    private WritableByteChannel mOutputChannel;
    private IOException mSinkException;
    private volatile boolean mStarted;
    private volatile boolean mCanceled;
    private volatile boolean mRecycled;
    private volatile boolean mFinished;
    private boolean mHeadersAvailable;
    private String mContentType;
    private long mContentLength;
    private long mUploadContentLength;
    private final ContextLock mLock;

    /**
     * Native peer object, owned by UrlRequest.
     */
    private long mUrlRequestPeer;

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
    public UrlRequest(UrlRequestContext requestContext, String url,
            int priority, Map<String, String> headers,
            WritableByteChannel sink) {
        if (requestContext == null) {
            throw new NullPointerException("Context is required");
        }
        if (url == null) {
            throw new NullPointerException("URL is required");
        }
        mRequestContext = requestContext;
        mUrl = url;
        mPriority = priority;
        mHeaders = headers;
        mSink = sink;
        mLock = new ContextLock();
        mUrlRequestPeer = nativeCreateRequestPeer(
                mRequestContext.getUrlRequestContextPeer(), mUrl, mPriority);
    }

    /**
     * Adds a request header.
     */
    public void addHeader(String header, String value) {
        validateNotStarted();
        if (mAdditionalHeaders == null) {
            mAdditionalHeaders = new HashMap<String, String>();
        }
        mAdditionalHeaders.put(header, value);
    }

    /**
     * Sets data to upload as part of a POST request.
     *
     * @param contentType MIME type of the post content or null if this is not a
     *            POST.
     * @param data The content that needs to be uploaded if this is a POST
     *            request.
     */
    public void setUploadData(String contentType, byte[] data) {
        synchronized (mLock) {
            validateNotStarted();
            mUploadContentType = contentType;
            mUploadData = data;
            mUploadChannel = null;
        }
    }

    /**
     * Sets a readable byte channel to upload as part of a POST request.
     *
     * @param contentType MIME type of the post content or null if this is not a
     *            POST request.
     * @param channel The channel to read to read upload data from if this is a
     *            POST request.
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
        }
    }

    public void setHttpMethod(String method) {
        validateNotStarted();
        if (!("PUT".equals(method) || "POST".equals(method))) {
            throw new IllegalArgumentException("Only PUT and POST are allowed.");
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
                      mUploadChannel != null)) {
                // Default to POST if there is data to upload but no method was
                // specified.
                method = "POST";
            }

            if (method != null) {
                nativeSetMethod(mUrlRequestPeer, method);
            }

            if (mHeaders != null && !mHeaders.isEmpty()) {
                for (Entry<String, String> entry : mHeaders.entrySet()) {
                    nativeAddHeader(mUrlRequestPeer, entry.getKey(),
                                    entry.getValue());
              }
            }

            if (mAdditionalHeaders != null) {
                for (Entry<String, String> entry :
                     mAdditionalHeaders.entrySet()) {
                    nativeAddHeader(mUrlRequestPeer, entry.getKey(),
                                    entry.getValue());
              }
            }

            if (mUploadData != null && mUploadData.length > 0) {
                nativeSetUploadData(mUrlRequestPeer, mUploadContentType,
                                    mUploadData);
            } else if (mUploadChannel != null) {
                nativeSetUploadChannel(mUrlRequestPeer, mUploadContentType,
                                       mUploadContentLength);
            }

            nativeStart(mUrlRequestPeer);
          }
    }

    public void cancel() {
        synchronized (mLock) {
            if (mCanceled) {
                return;
            }

            mCanceled = true;

            if (!mRecycled) {
                nativeCancel(mUrlRequestPeer);
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

    /**
     * Returns an exception if any, or null if the request was completed
     * successfully.
     */
    public IOException getException() {
        if (mSinkException != null) {
            return mSinkException;
        }

        validateNotRecycled();

        int errorCode = nativeGetErrorCode(mUrlRequestPeer);
        switch (errorCode) {
            case UrlRequestError.SUCCESS:
                return null;
            case UrlRequestError.UNKNOWN:
                return new IOException(nativeGetErrorString(mUrlRequestPeer));
            case UrlRequestError.MALFORMED_URL:
                return new MalformedURLException("Malformed URL: " + mUrl);
            case UrlRequestError.CONNECTION_TIMED_OUT:
                return new ConnectTimeoutException("Connection timed out");
            case UrlRequestError.UNKNOWN_HOST:
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

    public int getHttpStatusCode() {
        return nativeGetHttpStatusCode(mUrlRequestPeer);
    }

    /**
     * Content length as reported by the server. May be -1 or incorrect if the
     * server returns the wrong number, which happens even with Google servers.
     */
    public long getContentLength() {
        return mContentLength;
    }

    public String getContentType() {
        return mContentType;
    }

    public String getHeader(String name) {
        validateHeadersAvailable();
        return nativeGetHeader(mUrlRequestPeer, name);
    }

    // All response headers.
    public Map<String, List<String>> getAllHeaders() {
        validateHeadersAvailable();
        ResponseHeadersMap result = new ResponseHeadersMap();
        nativeGetAllHeaders(mUrlRequestPeer, result);
        return result;
    }

    /**
     * A callback invoked when the first chunk of the response has arrived.
     */
    @CalledByNative
    protected void onResponseStarted() {
        mContentType = nativeGetContentType(mUrlRequestPeer);
        mContentLength = nativeGetContentLength(mUrlRequestPeer);
        mHeadersAvailable = true;
    }

    /**
     * A callback invoked when the response has been fully consumed.
     */
    protected void onRequestComplete() {
    }

    /**
     * Consumes a portion of the response.
     *
     * @param byteBuffer The ByteBuffer to append. Must be a direct buffer, and
     *            no references to it may be retained after the method ends, as
     *            it wraps code managed on the native heap.
     */
    @CalledByNative
    protected void onBytesRead(ByteBuffer byteBuffer) {
        try {
            while (byteBuffer.hasRemaining()) {
                mSink.write(byteBuffer);
            }
        } catch (IOException e) {
            mSinkException = e;
            cancel();
        }
    }

    /**
     * Notifies the listener, releases native data structures.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void finish() {
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
            onRequestComplete();
            nativeDestroyRequestPeer(mUrlRequestPeer);
            mUrlRequestPeer = 0;
            mRecycled = true;
        }
    }

    /**
     * Appends header |name| with value |value| to |headersMap|.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onAppendResponseHeader(ResponseHeadersMap headersMap,
            String name, String value) {
        if (!headersMap.containsKey(name)) {
            headersMap.put(name, new ArrayList<String>());
        }
        headersMap.get(name).add(value);
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
        if (mUploadChannel == null || !mUploadChannel.isOpen())
            return -1;
        try {
            int result = mUploadChannel.read(dest);
            if (result < 0) {
                mUploadChannel.close();
                return 0;
            }
            return result;
        } catch (IOException e) {
            mSinkException = e;
            try {
                mUploadChannel.close();
            } catch (IOException ignored) {
                // Ignore this exception.
            }
            cancel();
            return -1;
        }
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

    public String getUrl() {
        return mUrl;
    }

    private native long nativeCreateRequestPeer(long urlRequestContextPeer,
            String url, int priority);

    private native void nativeAddHeader(long urlRequestPeer, String name,
            String value);

    private native void nativeSetMethod(long urlRequestPeer, String method);

    private native void nativeSetUploadData(long urlRequestPeer,
            String contentType, byte[] content);

    private native void nativeSetUploadChannel(long urlRequestPeer,
            String contentType, long contentLength);

    private native void nativeStart(long urlRequestPeer);

    private native void nativeCancel(long urlRequestPeer);

    private native void nativeDestroyRequestPeer(long urlRequestPeer);

    private native int nativeGetErrorCode(long urlRequestPeer);

    private native int nativeGetHttpStatusCode(long urlRequestPeer);

    private native String nativeGetErrorString(long urlRequestPeer);

    private native String nativeGetContentType(long urlRequestPeer);

    private native long nativeGetContentLength(long urlRequestPeer);

    private native String nativeGetHeader(long urlRequestPeer, String name);

    private native void nativeGetAllHeaders(long urlRequestPeer,
            ResponseHeadersMap headers);

    // Explicit class to work around JNI-generator generics confusion.
    private class ResponseHeadersMap extends HashMap<String, List<String>> {
    }
}
