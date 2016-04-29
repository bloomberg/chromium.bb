// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import org.chromium.base.VisibleForTesting;
import org.chromium.net.UploadDataProvider;
import org.chromium.net.UploadDataSink;

import java.io.IOException;
import java.net.HttpRetryException;
import java.net.ProtocolException;
import java.nio.ByteBuffer;

/**
 * An implementation of {@link java.io.OutputStream} to send data to a server,
 * when {@link CronetHttpURLConnection#setFixedLengthStreamingMode} is used.
 * This implementation does not buffer the entire request body in memory.
 * It does not support rewind. Note that {@link #write} should only be called
 * from the thread on which the {@link #mConnection} is created.
 */
final class CronetFixedModeOutputStream extends CronetOutputStream {
    // CronetFixedModeOutputStream buffers up to this value and wait for UploadDataStream
    // to consume the data. This field is non-final, so it can be changed for tests.
    // Using 16384 bytes is because the internal read buffer is 14520 for QUIC,
    // 16384 for SPDY, and 16384 for normal HTTP/1.1 stream.
    @VisibleForTesting
    private static int sDefaultBufferLength = 16384;
    private final CronetHttpURLConnection mConnection;
    private final MessageLoop mMessageLoop;
    private final long mContentLength;
    private final ByteBuffer mBuffer;
    private final UploadDataProvider mUploadDataProvider = new UploadDataProviderImpl();
    private long mBytesWritten;

    /**
     * Package protected constructor.
     * @param connection The CronetHttpURLConnection object.
     * @param contentLength The content length of the request body. Non-zero for
     *            non-chunked upload.
     */
    CronetFixedModeOutputStream(CronetHttpURLConnection connection,
            long contentLength, MessageLoop messageLoop) {
        if (connection == null) {
            throw new NullPointerException();
        }
        if (contentLength < 0) {
            throw new IllegalArgumentException(
                    "Content length must be larger than 0 for non-chunked upload.");
        }
        mContentLength = contentLength;
        int bufferSize = (int) Math.min(mContentLength, sDefaultBufferLength);
        mBuffer = ByteBuffer.allocate(bufferSize);
        mConnection = connection;
        mMessageLoop = messageLoop;
        mBytesWritten = 0;
    }

    @Override
    public void write(int oneByte) throws IOException {
        checkNotExceedContentLength(1);
        while (mBuffer.position() == mBuffer.limit()) {
            // Wait until buffer is consumed.
            mMessageLoop.loop();
        }
        mBuffer.put((byte) oneByte);
        mBytesWritten++;
        if (mBytesWritten == mContentLength) {
            // Entire post data has been received. Now wait for network stack to
            // read it.
            mMessageLoop.loop();
        }
    }

    @Override
    public void write(byte[] buffer, int offset, int count) throws IOException {
        if (buffer.length - offset < count || offset < 0 || count < 0) {
            throw new IndexOutOfBoundsException();
        }
        checkNotExceedContentLength(count);
        if (count == 0) {
            return;
        }
        int toSend = count;
        while (toSend > 0) {
            if (mBuffer.position() == mBuffer.limit()) {
                // Wait until buffer is consumed.
                mMessageLoop.loop();
            }
            int sent = Math.min(toSend, mBuffer.limit() - mBuffer.position());
            mBuffer.put(buffer, offset + count - toSend, sent);
            toSend -= sent;
        }
        mBytesWritten += count;
        if (mBytesWritten == mContentLength) {
            // Entire post data has been received. Now wait for network stack to
            // read it.
            mMessageLoop.loop();
        }
    }

    /**
     * Throws {@link java.net.ProtocolException} if adding {@code numBytes} will
     * exceed content length.
     */
    private void checkNotExceedContentLength(int numBytes) throws ProtocolException {
        if (mBytesWritten + numBytes > mContentLength) {
            throw new ProtocolException("expected "
                    + (mContentLength - mBytesWritten) + " bytes but received "
                    + numBytes);
        }
    }

    // TODO(xunjieli): implement close().

    // Below are CronetOutputStream implementations:

    @Override
    void setConnected() throws IOException {
        // Do nothing.
    }

    @Override
    void checkReceivedEnoughContent() throws IOException {
        if (mBytesWritten < mContentLength) {
            throw new ProtocolException("Content received is less than Content-Length.");
        }
    }

    @Override
    UploadDataProvider getUploadDataProvider() {
        return mUploadDataProvider;
    }

    private class UploadDataProviderImpl extends UploadDataProvider {
        @Override
        public long getLength() {
            return mContentLength;
        }

        @Override
        public void read(final UploadDataSink uploadDataSink, final ByteBuffer byteBuffer) {
            final int availableSpace = byteBuffer.remaining();
            if (availableSpace < mBuffer.position()) {
                // byteBuffer does not have enough capacity, so only put a portion
                // of mBuffer in it.
                byteBuffer.put(mBuffer.array(), 0, availableSpace);
                mBuffer.position(availableSpace);
                // Move remaining buffer to the head of the buffer for use in the
                // next read call.
                mBuffer.compact();
            } else {
                // byteBuffer has enough capacity to hold the content of mBuffer.
                mBuffer.flip();
                byteBuffer.put(mBuffer);
                // Reuse this buffer.
                mBuffer.clear();
                // Quit message loop so embedder can write more data.
                mMessageLoop.quit();
            }
            uploadDataSink.onReadSucceeded(false);
        }

        @Override
        public void rewind(UploadDataSink uploadDataSink) {
            uploadDataSink.onRewindError(
                    new HttpRetryException("Cannot retry streamed Http body", -1));
        }
    }

    /**
     * Sets the default buffer length for use in tests.
     */
    @VisibleForTesting
    static void setDefaultBufferLengthForTesting(int length) {
        sDefaultBufferLength = length;
    }
}
