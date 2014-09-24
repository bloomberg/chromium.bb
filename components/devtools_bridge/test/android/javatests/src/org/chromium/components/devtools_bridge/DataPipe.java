// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import java.nio.ByteBuffer;

/**
 * Represents a pair of connected AbstractDataChannel's. Sends to one channel
 * come to another and vice versa.
 */
public class DataPipe {
    private final SignalingThreadMock mSignalingThread = new SignalingThreadMock();

    private final PairedDataChannel mDC0 = new PairedDataChannel();
    private final PairedDataChannel mDC1 = new PairedDataChannel();

    public void connect() {
        mDC0.setPair(mDC1);
        mDC1.setPair(mDC0);
        mDC0.open();
        mDC1.open();
    }

    public void disconnect() {
        mDC0.setPair(null);
        mDC1.setPair(null);
        mDC0.close();
        mDC1.close();
    }

    public AbstractDataChannel dataChannel(int index) {
        switch (index) {
            case 0:
                return mDC0;
            case 1:
                return mDC1;
            default:
                throw new IllegalArgumentException("index");
        }
    }

    public void dispose() {
        mDC0.dispose();
        mDC1.dispose();
        mSignalingThread.dispose();
    }

    private class PairedDataChannel extends DataChannelMock {
        private PairedDataChannel mPair;

        public PairedDataChannel() {
            super(mSignalingThread);
        }

        public void setPair(final PairedDataChannel pair) {
            mSignalingThread.invoke(new Runnable() {
                @Override
                public void run() {
                    mPair = pair;
                }
            });
        }

        @Override
        protected void sendOnSignalingThread(ByteBuffer message) {
            assert message.remaining() > 0;

            if (mPair == null) return;
            mPair.notifyMessageOnSignalingThread(message);
        }

        @Override
        protected void disposeSignalingThread() {
            // Ignore. Will dispose in DataPipe.dispose.
        }
    }
}
