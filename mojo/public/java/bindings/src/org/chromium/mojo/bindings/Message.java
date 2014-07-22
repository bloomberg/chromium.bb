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
}
