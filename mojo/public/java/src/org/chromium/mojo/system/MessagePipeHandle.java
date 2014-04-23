// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system;

import java.nio.ByteBuffer;
import java.util.List;

/**
 * Message pipes are bidirectional communication channel for framed data (i.e., messages). Messages
 * can contain plain data and/or Mojo handles.
 */
public interface MessagePipeHandle extends Handle {
    /**
     * Flags for the write operations on MessagePipeHandle .
     */
    public static class WriteFlags extends Flags<WriteFlags> {
        private static final int FLAG_NONE = 0;

        /**
         * Dedicated constructor.
         *
         * @param flags initial value of the flag.
         */
        private WriteFlags(int flags) {
            super(flags);
        }

        /**
         * @return a flag with no bit set.
         */
        public static WriteFlags none() {
            return new WriteFlags(FLAG_NONE);
        }
    }

    /**
     * Flags for the read operations on MessagePipeHandle.
     */
    public static class ReadFlags extends Flags<ReadFlags> {
        private static final int FLAG_NONE = 0;
        private static final int FLAG_MAY_DISCARD = 1 << 0;

        /**
         * Dedicated constructor.
         *
         * @param flags initial value of the flag.
         */
        private ReadFlags(int flags) {
            super(flags);
        }

        /**
         * Change the may-discard bit of this flag. If set, if the message is unable to be read for
         * whatever reason (e.g., the caller-supplied buffer is too small), discard the message
         * (i.e., simply dequeue it).
         *
         * @param mayDiscard the new value of the may-discard bit.
         * @return this.
         */
        public ReadFlags setMayDiscard(boolean mayDiscard) {
            return setFlag(FLAG_MAY_DISCARD, mayDiscard);
        }

        /**
         * @return a flag with no bit set.
         */
        public static ReadFlags none() {
            return new ReadFlags(FLAG_NONE);
        }

    }

    /**
     * Writes a message to the message pipe endpoint, with message data specified by |bytes| and
     * attached handles specified by |handles|, and options specified by |flags|. If there is no
     * message data, |bytes| may be null, otherwise it must be a direct ByteBuffer. If there are no
     * attached handles, |handles| may be null.
     * <p>
     * If handles are attached, on success the handles will no longer be valid (the receiver will
     * receive equivalent, but logically different, handles). Handles to be sent should not be in
     * simultaneous use (e.g., on another thread).
     */
    void writeMessage(ByteBuffer bytes, List<Handle> handles, WriteFlags flags);

    /**
     * Result of the |readMessage| method.
     */
    public static class ReadMessageResult {
        /**
         * <code>true</code> if a message was read.
         */
        private boolean mWasMessageRead;
        /**
         * If a message was read, the size in bytes of the message, otherwise the size in bytes of
         * the next message.
         */
        private int mMessageSize;
        /**
         * If a message was read, the number of handles contained in the message, otherwise the
         * number of handles contained in the next message.
         */
        private int mHandlesCount;
        /**
         * If a message was read, the handles contained in the message, undefined otherwise.
         */
        private List<UntypedHandle> mHandles;

        /**
         * @return the wasMessageRead
         */
        public boolean getWasMessageRead() {
            return mWasMessageRead;
        }

        /**
         * @param wasMessageRead the wasMessageRead to set
         */
        public void setWasMessageRead(boolean wasMessageRead) {
            mWasMessageRead = wasMessageRead;
        }

        /**
         * @return the messageSize
         */
        public int getMessageSize() {
            return mMessageSize;
        }

        /**
         * @param messageSize the messageSize to set
         */
        public void setMessageSize(int messageSize) {
            mMessageSize = messageSize;
        }

        /**
         * @return the handlesCount
         */
        public int getHandlesCount() {
            return mHandlesCount;
        }

        /**
         * @param handlesCount the handlesCount to set
         */
        public void setHandlesCount(int handlesCount) {
            mHandlesCount = handlesCount;
        }

        /**
         * @return the handles
         */
        public List<UntypedHandle> getHandles() {
            return mHandles;
        }

        /**
         * @param handles the handles to set
         */
        public void setHandles(List<UntypedHandle> handles) {
            mHandles = handles;
        }
    }

    /**
     * Reads a message from the message pipe endpoint; also usable to query the size of the next
     * message or discard the next message. |bytes| indicate the buffer/buffer size to receive the
     * message data (if any) and |maxNumberOfHandles| indicate the maximum handle count to receive
     * the attached handles (if any). |bytes| is optional. If null, |maxNumberOfHandles| must be
     * zero, and the return value will indicate the size of the next message. If non-null, it must
     * be a direct ByteBuffer and the return value will indicate if the message was read or not. If
     * the message was read its content will be in |bytes|, and the attached handles will be in the
     * return value. Partial reads are NEVER done. Either a full read is done and |wasMessageRead|
     * will be true, or the read is NOT done and |wasMessageRead| will be false (if |mayDiscard| was
     * set, the message is also discarded in this case).
     */
    ReadMessageResult readMessage(ByteBuffer bytes, int maxNumberOfHandles,
            ReadFlags flags);
}
