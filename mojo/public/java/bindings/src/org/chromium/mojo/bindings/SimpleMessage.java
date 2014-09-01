// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.system.Handle;
import org.chromium.mojo.system.MessagePipeHandle;

import java.nio.ByteBuffer;
import java.util.List;

/**
 * A raw message to be sent/received from a {@link MessagePipeHandle}.
 */
public final class SimpleMessage implements Message {

    /**
     * The data of the message.
     */
    private final ByteBuffer mBuffer;

    /**
     * The handles of the message.
     */
    private final List<? extends Handle> mHandle;

    /**
     * This message interpreted with headers.
     */
    private MessageWithHeader mWithHeader = null;

    /**
     * Constructor.
     *
     * @param buffer The buffer containing the bytes to send. This must be a direct buffer.
     * @param handles The list of handles to send.
     */
    public SimpleMessage(ByteBuffer buffer, List<? extends Handle> handles) {
        assert buffer.isDirect();
        mBuffer = buffer;
        mHandle = handles;
    }

    /**
     * @see Message#getData()
     */
    @Override
    public ByteBuffer getData() {
        return mBuffer;
    }

    /**
     * @see Message#getHandles()
     */
    @Override
    public List<? extends Handle> getHandles() {
        return mHandle;
    }

    /**
     * @see Message#asMojoMessage()
     */
    @Override
    public MessageWithHeader asMojoMessage() {
        if (mWithHeader == null) {
            mWithHeader = new MessageWithHeader(this);
        }
        return mWithHeader;
    }
}
