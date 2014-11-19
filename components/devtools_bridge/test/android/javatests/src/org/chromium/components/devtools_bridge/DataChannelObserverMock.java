// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import java.nio.ByteBuffer;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingDeque;

/**
 * Mock for AbstractDataChannel.Observer.
 */
public class DataChannelObserverMock implements AbstractDataChannel.Observer {
    public final CountDownLatch opened = new CountDownLatch(1);
    public final CountDownLatch closed = new CountDownLatch(1);
    public final LinkedBlockingDeque<byte[]> received = new LinkedBlockingDeque<byte[]>();

    public void onStateChange(AbstractDataChannel.State state) {
        switch (state) {
            case OPEN:
                opened.countDown();
                break;

            case CLOSED:
                closed.countDown();
                break;
        }
    }

    public void onMessage(ByteBuffer message) {
        byte[] bytes = new byte[message.remaining()];
        message.get(bytes);
        received.add(bytes);
    }
}
