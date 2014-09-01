// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.system.Handle;
import org.chromium.mojo.system.MessagePipeHandle;

import java.nio.ByteBuffer;
import java.util.List;

/**
 * A raw message to be sent/received from a {@link MessagePipeHandle}. Note that this can contain
 * any data, not necessarily a Mojo message with a proper header. See also {@link MessageWithHeader}
 * and {@link SimpleMessage}.
 */
public interface Message {

    /**
     * The data part of the message.
     */
    public ByteBuffer getData();

    /**
     * The handles part of the message.
     */
    public List<? extends Handle> getHandles();

    /**
     * Returns the message considered as a message with a header.
     */
    public MessageWithHeader asMojoMessage();

}
