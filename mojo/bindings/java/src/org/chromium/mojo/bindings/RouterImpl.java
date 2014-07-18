// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.system.AsyncWaiter;
import org.chromium.mojo.system.MessagePipeHandle;

import java.util.HashMap;
import java.util.Map;

/**
 * Implementation of {@link Router}.
 */
public class RouterImpl implements Router {

    /**
     * {@link MessageReceiver} used as the {@link Connector} callback.
     */
    private class ResponderThunk implements MessageReceiver {

        /**
         * @see MessageReceiver#accept(MessageWithHeader)
         */
        @Override
        public boolean accept(MessageWithHeader message) {
            return handleIncomingMessage(message);
        }

    }

    /**
     * The {@link Connector} which is connected to the handle.
     */
    private final Connector mConnector;

    /**
     * The {@link MessageReceiverWithResponder} that will consume the messages received from the
     * pipe.
     */
    private MessageReceiverWithResponder mIncomingMessageReceiver;

    /**
     * The next id to use for a request id which needs a response. It is auto-incremented.
     */
    private long mNextRequestId = 1;

    /**
     * The map from request ids to {@link MessageReceiver} of request currently in flight.
     */
    private Map<Long, MessageReceiver> mResponders = new HashMap<Long, MessageReceiver>();

    /**
     * Constructor that will use the default {@link AsyncWaiter}.
     *
     * @param messagePipeHandle The {@link MessagePipeHandle} to route message for.
     */
    public RouterImpl(MessagePipeHandle messagePipeHandle) {
        this(messagePipeHandle, BindingsHelper.getDefaultAsyncWaiterForHandle(messagePipeHandle));
    }

    /**
     * Constructor.
     *
     * @param messagePipeHandle The {@link MessagePipeHandle} to route message for.
     * @param asyncWaiter the {@link AsyncWaiter} to use to get notification of new messages on the
     *            handle.
     */
    public RouterImpl(MessagePipeHandle messagePipeHandle, AsyncWaiter asyncWaiter) {
        mConnector = new Connector(messagePipeHandle, asyncWaiter);
        mConnector.setIncomingMessageReceiver(new ResponderThunk());
    }

    /**
     * @see org.chromium.mojo.bindings.Router#start()
     */
    @Override
    public void start() {
        mConnector.start();
    }

    /**
     * @see Router#setIncomingMessageReceiver(MessageReceiverWithResponder)
     */
    @Override
    public void setIncomingMessageReceiver(MessageReceiverWithResponder incomingMessageReceiver) {
        this.mIncomingMessageReceiver = incomingMessageReceiver;
    }

    /**
     * @see MessageReceiver#accept(MessageWithHeader)
     */
    @Override
    public boolean accept(MessageWithHeader message) {
        // A message without responder is directly forwarded to the connector.
        return mConnector.accept(message);
    }

    /**
     * @see MessageReceiverWithResponder#acceptWithResponder(MessageWithHeader, MessageReceiver)
     */
    @Override
    public boolean acceptWithResponder(MessageWithHeader message, MessageReceiver responder) {
        // Checking the message expects a response.
        assert message.getHeader().hasFlag(MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG);

        // Compute a request id for being able to route the response.
        long requestId = mNextRequestId++;
        // Reserve 0 in case we want it to convey special meaning in the future.
        if (requestId == 0) {
            requestId = mNextRequestId++;
        }
        if (mResponders.containsKey(requestId)) {
            throw new IllegalStateException("Unable to find a new request identifier.");
        }
        message.setRequestId(requestId);
        if (!mConnector.accept(message)) {
            return false;
        }
        // Only keep the responder is the message has been accepted.
        mResponders.put(requestId, responder);
        return true;
    }

    /**
     * @see org.chromium.mojo.bindings.HandleOwner#passHandle()
     */
    @Override
    public MessagePipeHandle passHandle() {
        return mConnector.passHandle();
    }

    /**
     * @see java.io.Closeable#close()
     */
    @Override
    public void close() {
        mConnector.close();
    }

    /**
     * @see Router#setErrorHandler(ConnectionErrorHandler)
     */
    @Override
    public void setErrorHandler(ConnectionErrorHandler errorHandler) {
        mConnector.setErrorHandler(errorHandler);
    }

    /**
     * Receive a message from the connector. Returns |true| if the message has been handled.
     */
    private boolean handleIncomingMessage(MessageWithHeader message) {
        MessageHeader header = message.getHeader();
        if (header.hasFlag(MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG)) {
            if (mIncomingMessageReceiver != null) {
                return mIncomingMessageReceiver.acceptWithResponder(message, this);
            }
            // If we receive a request expecting a response when the client is not
            // listening, then we have no choice but to tear down the pipe.
            close();
            return false;
        } else if (header.hasFlag(MessageHeader.MESSAGE_IS_RESPONSE_FLAG)) {
            long requestId = header.getRequestId();
            MessageReceiver responder = mResponders.get(requestId);
            if (responder == null) {
                return false;
            }
            return responder.accept(message);
        } else {
            if (mIncomingMessageReceiver != null) {
                return mIncomingMessageReceiver.accept(message);
            }
            // OK to drop the message.
        }
        return false;
    }
}
