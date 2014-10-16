// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.Assert;

import org.chromium.components.devtools_bridge.SessionControlMessages.ClientMessage;
import org.chromium.components.devtools_bridge.SessionControlMessages.ClientMessageHandler;
import org.chromium.components.devtools_bridge.SessionControlMessages.IceExchangeMessage;
import org.chromium.components.devtools_bridge.SessionControlMessages.InvalidFormatException;
import org.chromium.components.devtools_bridge.SessionControlMessages.Message;
import org.chromium.components.devtools_bridge.SessionControlMessages.MessageHandler;
import org.chromium.components.devtools_bridge.SessionControlMessages.ServerMessage;
import org.chromium.components.devtools_bridge.SessionControlMessages.ServerMessageHandler;
import org.chromium.components.devtools_bridge.SessionControlMessages.UnknownRequestMessage;
import org.chromium.components.devtools_bridge.SessionControlMessages.UnknownResponseMessage;

/**
 * Tests for {@link SessionControlMessages}
 */
public class SessionControlMessagesTest extends InstrumentationTestCase {
    private static final String UNKNOWN_REQUEST_TYPE = "@unknown request@";
    private static final String UNKNOWN_REQUEST = "{\"type\": \"" + UNKNOWN_REQUEST_TYPE + "\"}";

    @SmallTest
    public void testIceExchangeMessage() {
        recode(new IceExchangeMessage());
    }

    @SmallTest
    public void testUnknownRequest() throws InvalidFormatException {
        UnknownRequestMessage request =
                (UnknownRequestMessage) ClientMessageReader.readMessage(UNKNOWN_REQUEST);
        UnknownResponseMessage response = ServerMessageReader.recode(request.createResponse());
        Assert.assertEquals(UNKNOWN_REQUEST_TYPE, response.rawRequestType);
    }

    private <T extends ServerMessage> T recode(T message) {
        assert message != null;
        return ServerMessageReader.recode(message);
    }

    @SuppressWarnings("unchecked")
    private static <T> T cast(T prototype, Object object) {
        Assert.assertNotNull(object);
        if (prototype.getClass() == object.getClass())
            return (T) object;
        else
            throw new ClassCastException();
    }

    private static void checkedRead(MessageHandler handler, Message<?> message) {
        try {
            handler.readMessage(SessionControlMessages.toByteArray(message));
        } catch (InvalidFormatException e) {
            Assert.fail(e.toString());
        }
    }

    private static class ServerMessageReader extends ServerMessageHandler {
        private ServerMessage mLastMessage;

        @Override
        protected void onMessage(ServerMessage message) {
            mLastMessage = message;
        }

        public static <T extends ServerMessage> T recode(T message) {
            ServerMessageReader handler = new ServerMessageReader();
            checkedRead(handler, message);
            return cast(message, handler.mLastMessage);
        }
    }

    private static class ClientMessageReader extends ClientMessageHandler {
        private ClientMessage mLastMessage;

        @Override
        protected void onMessage(ClientMessage message) {
            mLastMessage = message;
        }

        public static ClientMessage readMessage(String json) throws InvalidFormatException {
            ClientMessageReader reader = new ClientMessageReader();
            reader.readMessage(json.getBytes());
            return reader.mLastMessage;
        }
    }
}
