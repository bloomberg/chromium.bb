// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.TestUtils;
import org.chromium.mojo.system.Handle;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.Pair;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

/**
 * Utility class for bindings tests.
 */
public class BindingsTestUtils {

    /**
     * {@link MessageReceiver} that records any message it receives.
     */
    public static class RecordingMessageReceiver implements MessageReceiver {

        public final List<MessageWithHeader> messages = new ArrayList<MessageWithHeader>();

        /**
         * @see MessageReceiver#accept(MessageWithHeader)
         */
        @Override
        public boolean accept(MessageWithHeader message) {
            messages.add(message);
            return true;
        }
    }

    /**
     * {@link MessageReceiverWithResponder} that records any message it receives.
     */
    public static class RecordingMessageReceiverWithResponder extends RecordingMessageReceiver
            implements MessageReceiverWithResponder {

        public final List<Pair<MessageWithHeader, MessageReceiver>> messagesWithReceivers =
                new ArrayList<Pair<MessageWithHeader, MessageReceiver>>();

        /**
         * @see MessageReceiverWithResponder#acceptWithResponder(MessageWithHeader,
         *      MessageReceiver)
         */
        @Override
        public boolean acceptWithResponder(MessageWithHeader message, MessageReceiver responder) {
            messagesWithReceivers.add(Pair.create(message, responder));
            return true;
        }
    }

    /**
     * {@link ConnectionErrorHandler} that records any error it received.
     */
    public static class CapturingErrorHandler implements ConnectionErrorHandler {

        public MojoException exception = null;

        /**
         * @see ConnectionErrorHandler#onConnectionError(MojoException)
         */
        @Override
        public void onConnectionError(MojoException e) {
            exception = e;
        }
    }

    /**
     * Creates a new valid {@link MessageWithHeader}.
     */
    public static MessageWithHeader newRandomMessageWithHeader(int size) {
        assert size > 16;
        ByteBuffer message = TestUtils.newRandomBuffer(size);
        int[] headerAsInts = { 16, 2, 0, 0 };
        for (int i = 0; i < 4; ++i) {
            message.putInt(4 * i, headerAsInts[i]);
        }
        message.position(0);
        return new MessageWithHeader(new Message(message, new ArrayList<Handle>()));
    }
}
