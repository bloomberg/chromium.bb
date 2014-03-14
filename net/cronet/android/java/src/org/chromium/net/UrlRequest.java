// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.apache.http.conn.ConnectTimeoutException;
import org.chromium.base.AccessedByNative;
import org.chromium.base.CalledByNative;

import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.nio.channels.ReadableByteChannel;
import java.nio.channels.WritableByteChannel;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;
import java.util.concurrent.Semaphore;

/**
 * Network request using the native http stack implementation.
 */
public class UrlRequest {
  private static final class ContextLock {
  }

  /**
   * Must match definitions in the native source.
   */
  public static final int REQUEST_PRIORITY_IDLE = 0;
  public static final int REQUEST_PRIORITY_LOWEST = 1;
  public static final int REQUEST_PRIORITY_LOW = 2;
  public static final int REQUEST_PRIORITY_MEDIUM = 3;
  public static final int REQUEST_PRIORITY_HIGHEST = 4;

  /**
   * Must match definitions in the native source.
   */
  private static final int SUCCESS = 0;
  private static final int UNKNOWN_ERROR = 1;
  private static final int MALFORMED_URL_ERROR = 2;
  private static final int CONNECTION_TIMED_OUT = 3;
  private static final int UNKNOWN_HOST = 4;

  private static final int UPLOAD_BYTE_BUFFER_SIZE = 32768;

  // TODO(mef) : Rename to follow Google Java style guide:
  // In Google Style special prefixes or suffixes, like those seen in the
  // examples name_, mName, s_name and kName, are not used.
  private final UrlRequestContext mRequestContext;
  private final String mUrl;
  private final int mPriority;
  private final Map<String, String> mHeaders;
  private final WritableByteChannel mSink;

  private Map<String, String> mAdditionalHeaders;
  private boolean mPostBodySet;
  private ReadableByteChannel mPostBodyChannel;
  private WritableByteChannel mOutputChannel;
  private IOException mSinkException;
  private volatile boolean mStarted;
  private volatile boolean mCanceled;
  private volatile boolean mRecycled;
  private volatile boolean mFinished;
  private String mContentType;
  private long mContentLength;
  private Semaphore mAppendChunkSemaphore;
  private final ContextLock mLock;

  /**
   * This field is accessed exclusively from the native layer.
   */
  @AccessedByNative
  @SuppressWarnings("unused")
  private long mRequest;

  /**
   * Constructor.
   *
   * @param requestContext The context.
   * @param url The URL.
   * @param priority Request priority, e.g. {@link #REQUEST_PRIORITY_MEDIUM}.
   * @param headers HTTP headers.
   * @param sink The output channel into which downloaded content will be
   *        written.
   */
  public UrlRequest(UrlRequestContext requestContext,
                    String url,
                    int priority,
                    Map<String, String> headers,
                    WritableByteChannel sink) {
    mRequestContext = requestContext;
    mUrl = url;
    mPriority = priority;
    mHeaders = headers;
    mSink = sink;
    mLock = new ContextLock();
    nativeInit(mRequestContext, mUrl, mPriority);
    mPostBodySet = false;
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
   *        POST.
   * @param data The content that needs to be uploaded if this is a POST
   *        request.
   */
  public void setUploadData(String contentType, byte[] data) {
    synchronized (mLock) {
      validateNotStarted();
      validatePostBodyNotSet();
      nativeSetPostData(contentType, data);
      mPostBodySet = true;
    }
  }

  /**
   * Sets a readable byte channel to upload as part of a POST request.
   *
   * @param contentType MIME type of the post content or null if this is not a
   *        POST request.
   * @param channel The channel to read to read upload data from if this is a
   *        POST request.
   */
  public void setUploadChannel(String contentType,
                               ReadableByteChannel channel) {
    synchronized (mLock) {
      validateNotStarted();
      validatePostBodyNotSet();
      nativeBeginChunkedUpload(contentType);
      mPostBodyChannel = channel;
      mPostBodySet = true;
    }
    mAppendChunkSemaphore = new Semaphore(0);
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

      if (mHeaders != null && !mHeaders.isEmpty()) {
        for (Entry<String, String> entry : mHeaders.entrySet()) {
          nativeAddHeader(entry.getKey(), entry.getValue());
        }
      }

      if (mAdditionalHeaders != null && !mAdditionalHeaders.isEmpty()) {
        for (Entry<String, String> entry : mAdditionalHeaders.entrySet()) {
          nativeAddHeader(entry.getKey(), entry.getValue());
        }
      }

      nativeStart();
    }

    if (mPostBodyChannel != null) {
      uploadFromChannel(mPostBodyChannel);
    }
  }

  /**
   * Uploads data from a {@code ReadableByteChannel} using chunked transfer
   * encoding.
   *
   * The native call to append a chunk is asynchronous so a semaphore is used
   * to delay writing into the buffer again until chromium is finished with it.
   *
   * @param channel the channel to read data from.
   */
  private void uploadFromChannel(ReadableByteChannel channel) {
    ByteBuffer buffer = ByteBuffer.allocateDirect(UPLOAD_BYTE_BUFFER_SIZE);

    // The chromium API requires us to specify in advance if a chunk is the last
    // one. This extra ByteBuffer is needed to peek ahead and check for the end
    // of the channel.
    ByteBuffer checkForEnd = ByteBuffer.allocate(1);

    try {
      boolean lastChunk;
      do {
        // First dump in the one byte we read to check for the end of the
        // channel. (The first time through the loop the checkForEnd buffer will
        // be empty).
        checkForEnd.flip();
        buffer.clear();
        buffer.put(checkForEnd);
        checkForEnd.clear();

        channel.read(buffer);
        lastChunk = channel.read(checkForEnd) <= 0;
        buffer.flip();
        nativeAppendChunk(buffer, buffer.limit(), lastChunk);

        if (lastChunk) {
          break;
        }

        // Acquire permit before writing to the buffer again to ensure chromium
        // is done with it.
        mAppendChunkSemaphore.acquire();

      } while (!lastChunk && !mFinished);

    } catch (IOException e) {
      mSinkException = e;
      cancel();
    } catch (InterruptedException e) {
      mSinkException = new IOException(e);
      cancel();
    } finally {
      try {
        mPostBodyChannel.close();
      } catch (IOException ignore) {
        ;
      }
    }
  }

  public void cancel() {
    synchronized (mLock) {
      if (mCanceled) {
        return;
      }

      mCanceled = true;

      if (!mRecycled) {
        nativeCancel();
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

    int errorCode = nativeGetErrorCode();
    switch (errorCode) {
      case SUCCESS:
        return null;
      case UNKNOWN_ERROR:
        return new IOException(nativeGetErrorString());
      case MALFORMED_URL_ERROR:
        return new MalformedURLException("Malformed URL: " + mUrl);
      case CONNECTION_TIMED_OUT:
        return new ConnectTimeoutException("Connection timed out");
      case UNKNOWN_HOST:
        String host;
        try {
          host = new URL(mUrl).getHost();
        } catch (MalformedURLException e) {
          host = mUrl;
        }
        return new UnknownHostException("Unknown host: " + host);
      default:
        throw new IllegalStateException("Unrecognized error code: " +
                                        errorCode);
    }
  }

  public native int getHttpStatusCode();

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

  /**
   * A callback invoked when appending a chunk to the request has completed.
   */
  @CalledByNative
  protected void onAppendChunkCompleted() {
    mAppendChunkSemaphore.release();
  }

  /**
   * A callback invoked when the first chunk of the response has arrived.
   */
  @CalledByNative
  protected void onResponseStarted() {
    mContentType = nativeGetContentType();
    mContentLength = nativeGetContentLength();
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
   *        no references to it may be retained after the method ends, as it
   *        wraps code managed on the native heap.
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
      if (mAppendChunkSemaphore != null) {
        mAppendChunkSemaphore.release();
      }

      if (mRecycled) {
        return;
      }
      try {
        mSink.close();
      } catch (IOException e) {
        // Ignore
      }
      onRequestComplete();
      nativeRecycle();
      mRecycled = true;
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

  private void validatePostBodyNotSet() {
    if (mPostBodySet) {
      throw new IllegalStateException("Post Body already set");
    }
  }

  public String getUrl() {
    return mUrl;
  }

  private native void nativeInit(UrlRequestContext requestContext,
                                 String url,
                                 int priority);
  private native void nativeAddHeader(String name, String value);
  private native void nativeSetPostData(String contentType, byte[] content);
  private native void nativeBeginChunkedUpload(String contentType);
  private native void nativeAppendChunk(ByteBuffer chunk,
                                        int chunkSize,
                                        boolean isLastChunk);
  private native void nativeStart();
  private native void nativeCancel();
  private native void nativeRecycle();
  private native int nativeGetErrorCode();
  private native String nativeGetErrorString();
  private native String nativeGetContentType();
  private native long nativeGetContentLength();
}
