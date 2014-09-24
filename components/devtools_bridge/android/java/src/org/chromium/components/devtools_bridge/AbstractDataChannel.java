// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import java.nio.ByteBuffer;

/**
 * Limited view on org.webrtc.DataChannel. Abstraction layer helps with:
 * 1. Mocking in tests. There is no need to emulate full set of features of the DataChannel.
 * 2. Allows both native and Java API implementation for WebRTC data channel.
 * 3. Hides unused features.
 * Only SCTP data channels supported.
 * Data channel is thread safe (except the dispose method).
 */
public abstract class AbstractDataChannel {
    /**
     *  Observer's callbacks are called on WebRTC signaling thread (or it's equivalent in tests).
     */
    public interface Observer {
        void onStateChange(State state);

        /**
         * TEXT and BINARY messages should be handled equally. Size of the message is
         * |message|.remaining(). |message| may reference to a native buffer on stack so
         * the reference to the buffer must not outlive the invocation.
         */
        void onMessage(ByteBuffer message);
    }

    /**
     * Type is only significant for JavaScript-based counterpart. TEXT messages will
     * be observed as strings, BINARY as ByteArray's.
     */
    public enum MessageType {
        TEXT, BINARY
    }

    /**
     * State of the data channel.
     * Only 2 states of channel are important here: OPEN and everything else.
     */
    public enum State {
        OPEN, CLOSED
    }

    /**
     * Registers an observer.
     */
    public abstract void registerObserver(Observer observer);

    /**
     * Unregisters the previously registered observer.
     * Observer unregistration synchronized with signaling thread. If some data modified
     * in observer callbacks without additional synchronization it's safe to access
     * this data on the current thread after calling this method.
     */
    public abstract void unregisterObserver();

    /**
     * Sending message to the data channel.
     * Message size is |message|.remaining().
     */
    public abstract void send(ByteBuffer message, MessageType type);

    /**
     * Closing data channel. Both channels in the pair will change state to CLOSED.
     */
    public abstract void close();

    /**
     * Releases native objects (if any). Closes data channel. No other methods are allowed after it
     * (in multithread scenario needs synchronization with access to the data channel).
     */
    public abstract void dispose();
}
