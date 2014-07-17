// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MessagePipeHandle.ReadMessageResult;
import org.chromium.mojo.system.MojoResult;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Represents a {@link Message} which contains a {@link MessageHeader}. Deals with parsing the
 * {@link MessageHeader} for a message.
 */
public class MessageWithHeader {

    private final Message mBaseMessage;
    private final MessageHeader mHeader;
    private Message mPayload;

    /**
     * Reinterpret the given |message| as a message with the given |header|. The |message| must
     * contain the |header| as the start of its raw data.
     */
    public MessageWithHeader(Message baseMessage, MessageHeader header) {
        assert header.equals(new org.chromium.mojo.bindings.MessageHeader(baseMessage));
        this.mBaseMessage = baseMessage;
        this.mHeader = header;
    }

    /**
     * Reinterpret the given |message| as a message with a header. The |message| must contain a
     * header as the start of it's raw data, which will be parsed by this constructor.
     */
    public MessageWithHeader(Message baseMessage) {
        this(baseMessage, new org.chromium.mojo.bindings.MessageHeader(baseMessage));
    }

    /**
     * Returns the header of the given message. This will throw a {@link DeserializationException}
     * if the start of the message is not a valid header.
     */
    public MessageHeader getHeader() {
        return mHeader;
    }

    /**
     * Returns the payload of the message.
     */
    public Message getPayload() {
        if (mPayload == null) {
            ByteBuffer truncatedBuffer = ((ByteBuffer) mBaseMessage.buffer.position(
                    getHeader().getSize())).slice();
            truncatedBuffer.order(ByteOrder.nativeOrder());
            mPayload = new Message(truncatedBuffer, mBaseMessage.handles);
        }
        return mPayload;
    }

    /**
     * Returns the raw message.
     */
    public Message getMessage() {
        return mBaseMessage;
    }

    /**
     * Set the request identifier on the message.
     */
    void setRequestId(long requestId) {
        mHeader.setRequestId(mBaseMessage.buffer, requestId);
    }

    /**
     * Read a message, and pass it to the given |MessageReceiver| if not null. If the
     * |MessageReceiver| is null, the message is lost.
     *
     * @param receiver The {@link MessageReceiver} that will receive the read {@link Message}. Can
     *            be <code>null</code>, in which case the message is discarded.
     */
    public static int readAndDispatchMessage(MessagePipeHandle handle, MessageReceiver receiver) {
        // TODO(qsr) Allow usage of a pool of pre-allocated buffer for performance.
        ReadMessageResult result = handle.readMessage(null, 0, MessagePipeHandle.ReadFlags.NONE);
        if (result.getMojoResult() != MojoResult.RESOURCE_EXHAUSTED) {
            return result.getMojoResult();
        }
        ByteBuffer buffer = ByteBuffer.allocateDirect(result.getMessageSize());
        result = handle.readMessage(buffer, result.getHandlesCount(),
                MessagePipeHandle.ReadFlags.NONE);
        if (receiver != null && result.getMojoResult() == MojoResult.OK) {
            receiver.accept(new MessageWithHeader(new Message(buffer, result.getHandles())));
        }
        return result.getMojoResult();
    }
}
