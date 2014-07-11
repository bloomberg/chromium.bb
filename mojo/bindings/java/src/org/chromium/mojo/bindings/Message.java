// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.system.Handle;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MessagePipeHandle.ReadMessageResult;
import org.chromium.mojo.system.MojoResult;

import java.nio.ByteBuffer;
import java.util.List;

/**
 * A raw message to be sent/received from a {@link MessagePipeHandle}.
 */
public final class Message {

    /**
     * The data of the message.
     */
    public final ByteBuffer buffer;

    /**
     * The handles of the message.
     */
    public final List<? extends Handle> handles;

    /**
     * Constructor.
     *
     * @param buffer The buffer containing the bytes to send. This must be a direct buffer.
     * @param handles The list of handles to send.
     */
    public Message(ByteBuffer buffer, List<? extends Handle> handles) {
        assert buffer.isDirect();
        this.buffer = buffer;
        this.handles = handles;
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
            receiver.accept(new Message(buffer, result.getHandles()));
        }
        return result.getMojoResult();
    }
}
