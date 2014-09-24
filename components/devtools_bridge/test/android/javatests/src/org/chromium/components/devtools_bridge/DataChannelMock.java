// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import junit.framework.Assert;

import java.nio.ByteBuffer;
import java.util.concurrent.LinkedBlockingQueue;

/**
 * Mock of AbstractDataChannel tests. Also base class for DataPipe channels.
 */
public class DataChannelMock extends AbstractDataChannel {
    private final SignalingThreadMock mSignalingThread;
    private Observer mObserver;
    private boolean mOpen = false;
    private final LinkedBlockingQueue<ByteBuffer> mQueue = new LinkedBlockingQueue<ByteBuffer>();

    // |signalingThread| will be disposed in the |dispose| method. If successor needs
    // to control it's lifetime it must override |disposeSignalingThread| and not to invoke super's
    // implementation.
    protected DataChannelMock(SignalingThreadMock signalingThread) {
        mSignalingThread = signalingThread;
    }

    public DataChannelMock() {
        this(new SignalingThreadMock());
    }

    public void open() {
        onStateChange(AbstractDataChannel.State.OPEN);
    }

    @Override
    public void close() {
        onStateChange(AbstractDataChannel.State.CLOSED);
    }

    private void onStateChange(final State state) {
        mSignalingThread.invoke(new Runnable() {
            @Override
            public void run() {
                mObserver.onStateChange(state);
            }
        });
    }

    // Sends onMessage to the observer.
    public void notifyMessage(ByteBuffer data) {
        final byte[] bytes = toByteArray(data);
        mSignalingThread.invoke(new Runnable() {
            @Override
            public void run() {
                notifyMessageOnSignalingThread(ByteBuffer.wrap(bytes));
            }
        });
    }

    protected void notifyMessageOnSignalingThread(ByteBuffer buffer) {
        mObserver.onMessage(buffer);
    }

    // Blocks until message received. Removes it from the queue and returns.
    public ByteBuffer receive() {
        try {
            return mQueue.take();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public void registerObserver(final Observer observer) {
        mSignalingThread.invoke(new Runnable() {
        @Override
            public void run() {
                Assert.assertNull(mObserver);
                mObserver = observer;
                Assert.assertNotNull(mObserver);
            }
        });
    }

    @Override
    public void unregisterObserver() {
        mSignalingThread.invoke(new Runnable() {
        @Override
            public void run() {
                Assert.assertNotNull(mObserver);
                mObserver = null;
            }
        });
    }

    private byte[] toByteArray(ByteBuffer data) {
        final byte[] result = new byte[data.remaining()];
        data.get(result);
        return result;
    }

    @Override
    public void send(ByteBuffer message, MessageType type) {
        final byte[] data = toByteArray(message);
        assert data.length > 0;
        mSignalingThread.post(new Runnable() {
            @Override
            public void run() {
                sendOnSignalingThread(ByteBuffer.wrap(data));
                android.util.Log.d("DataChannelMock", "Packet sent.");
            }
        });
    }

    protected void sendOnSignalingThread(ByteBuffer message) {
        boolean success = mQueue.offer(message);
        assert success;
    }

    @Override
    public void dispose() {
        mSignalingThread.invoke(new Runnable() {
            @Override
            public void run() {
                Assert.assertNull(mObserver);
            }
        });
        disposeSignalingThread();
    }

    protected void disposeSignalingThread() {
        mSignalingThread.dispose();
    }
}
