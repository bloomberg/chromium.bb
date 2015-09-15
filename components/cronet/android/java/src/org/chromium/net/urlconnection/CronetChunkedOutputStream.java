// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import org.chromium.net.UploadDataProvider;
import org.chromium.net.UploadDataSink;

import java.io.IOException;
import java.net.HttpRetryException;
import java.nio.ByteBuffer;

/**
 * An implementation of {@link java.io.OutputStream} to send data to a server,
 * when {@link CronetHttpURLConnection#setChunkedStreamingMode} is used.
 * This implementation does not buffer the entire request body in memory.
 * It does not support rewind. Note that {@link #write} should only be called
 * from the thread on which the {@link #mConnection} is created.
 */
final class CronetChunkedOutputStream extends CronetOutputStream {
    private final CronetHttpURLConnection mConnection;
    private final MessageLoop mMessageLoop;
    private final ByteBuffer mBuffer;
    private final UploadDataProvider mUploadDataProvider = new UploadDataProviderImpl();
    private long mBytesWritten;
    private boolean mLastChunk = false;
    private boolean mClosed = false;

    /**
     * Package protected constructor.
     * @param connection The CronetHttpURLConnection object.
     * @param contentLength The content length of the request body. Non-zero for
     *            non-chunked upload.
     */
    CronetChunkedOutputStream(
            CronetHttpURLConnection connection, int chunkLength, MessageLoop messageLoop) {
        if (connection == null) {
            throw new NullPointerException();
        }
        if (chunkLength <= 0) {
            throw new IllegalArgumentException("chunkLength should be greater than 0");
        }
        mBuffer = ByteBuffer.allocate(chunkLength);
        mConnection = connection;
        mMessageLoop = messageLoop;
        mBytesWritten = 0;
    }

    @Override
    public void write(int oneByte) throws IOException {
        checkNotClosed();
        while (mBuffer.position() == mBuffer.limit()) {
            // Wait until buffer is consumed.
            mMessageLoop.loop();
        }
        mBuffer.put((byte) oneByte);
        mBytesWritten++;
    }

    @Override
    public void write(byte[] buffer, int offset, int count) throws IOException {
        checkNotClosed();
        if (buffer.length - offset < count || offset < 0 || count < 0) {
            throw new IndexOutOfBoundsException();
        }
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
    }

    @Override
    public void close() throws IOException {
        if (!mLastChunk) {
            // Write last chunk.
            mLastChunk = true;
            mMessageLoop.loop();
        }
        mClosed = true;
    }

    private void checkNotClosed() throws IOException {
        if (mClosed) {
            throw new IOException("Stream has been closed.");
        }
    }

    // Below are CronetOutputStream implementations:

    @Override
    void setConnected() throws IOException {
        // Do nothing.
    }

    @Override
    void checkReceivedEnoughContent() throws IOException {
        // Do nothing.
    }

    @Override
    UploadDataProvider getUploadDataProvider() {
        return mUploadDataProvider;
    }

    private class UploadDataProviderImpl extends UploadDataProvider {
        @Override
        public long getLength() {
            return -1;
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
                uploadDataSink.onReadSucceeded(false);
            } else {
                // byteBuffer has enough capacity to hold the content of mBuffer.
                mBuffer.flip();
                byteBuffer.put(mBuffer);
                // Reuse this buffer.
                mBuffer.clear();
                // Quit message loop so embedder can write more data.
                mMessageLoop.quit();
                uploadDataSink.onReadSucceeded(mLastChunk);
            }
        }

        @Override
        public void rewind(UploadDataSink uploadDataSink) {
            uploadDataSink.onRewindError(
                    new HttpRetryException("Cannot retry streamed Http body", -1));
        }
    }
}
