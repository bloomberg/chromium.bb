// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.system.Handle;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.List;

/**
 * Represents a {@link Message} which contains a {@link MessageHeader}. Deals with parsing the
 * {@link MessageHeader} for a message.
 */
public class MessageWithHeader implements Message {

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
    MessageWithHeader(Message baseMessage) {
        this(baseMessage, new org.chromium.mojo.bindings.MessageHeader(baseMessage));
    }

    /**
     * @see Message#getData()
     */
    @Override
    public ByteBuffer getData() {
        return mBaseMessage.getData();
    }

    /**
     * @see Message#getHandles()
     */
    @Override
    public List<? extends Handle> getHandles() {
        return mBaseMessage.getHandles();
    }

    /**
     * @see Message#asMojoMessage()
     */
    @Override
    public MessageWithHeader asMojoMessage() {
        return this;
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
            ByteBuffer truncatedBuffer = ((ByteBuffer) mBaseMessage.getData().position(
                    getHeader().getSize())).slice();
            truncatedBuffer.order(ByteOrder.nativeOrder());
            mPayload = new SimpleMessage(truncatedBuffer, mBaseMessage.getHandles());
        }
        return mPayload;
    }

    /**
     * Set the request identifier on the message.
     */
    void setRequestId(long requestId) {
        mHeader.setRequestId(mBaseMessage.getData(), requestId);
    }

}
