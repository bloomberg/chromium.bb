// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.util.JsonReader;
import android.util.JsonWriter;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;

/**
 * Defines protocol of control channel of SessionBase. Messages are JSON serializable
 * and transferred through AbstractDataChannel.
 */
public final class SessionControlMessages {
    private SessionControlMessages() {
        throw new RuntimeException("Class not intended to instantiate");
    }

    /**
     * Types of messages that client sends to server.
     */
    enum ClientMessageType {
        UNKNOWN_REQUEST
    }

    /**
     * Types of messages that servers sends to client.
     */
    enum ServerMessageType {
        ICE_EXCHANGE,
        UNKNOWN_RESPONSE
    }

    /**
     * Base class for all messages.
     */
    public abstract static class Message<T extends Enum> {
        public final T type;

        protected Message(T type) {
            this.type = type;
        }

        public void write(JsonWriter writer) throws IOException {
            writer.name("type");
            writer.value(type.toString());
        }
    }

    /**
     * Base calss for messages that client sends to server.
     */
    public abstract static class ClientMessage extends Message<ClientMessageType> {
        protected ClientMessage(ClientMessageType type) {
            super(type);
        }
    }

    /**
     * Base class for messages that server sends to client.
     */
    public abstract static class ServerMessage extends Message<ServerMessageType> {
        protected ServerMessage(ServerMessageType type) {
            super(type);
        }
    }

    /**
     * Server sends this message when it has ICE candidates to exchange. Client initiates
     * ICE exchange over signaling channel.
     */
    public static final class IceExchangeMessage extends ServerMessage {
        public IceExchangeMessage() {
            super(ServerMessageType.ICE_EXCHANGE);
        }
    }

    /**
     * Server response on unrecognized client message.
     */
    public static final class UnknownResponseMessage extends ServerMessage {
        public final String rawRequestType;

        public UnknownResponseMessage(String rawRequestType) {
            super(ServerMessageType.UNKNOWN_RESPONSE);
            this.rawRequestType = rawRequestType;
        }

        @Override
        public void write(JsonWriter writer) throws IOException {
            super.write(writer);
            writer.name("rawRequestType");
            writer.value(rawRequestType.toString());
        }
    }

    /**
     * Helper class to represent message of unknown type. Should not be sent.
     */
    public static final class UnknownRequestMessage extends ClientMessage {
        public final String rawType;

        public UnknownRequestMessage(String rawType) {
            super(ClientMessageType.UNKNOWN_REQUEST);
            this.rawType = rawType;
        }

        @Override
        public void write(JsonWriter writer) throws IOException {
            throw new RuntimeException("Should not be serialized");
        }

        public UnknownResponseMessage createResponse() {
            return new UnknownResponseMessage(rawType);
        }
    }

    private static <T extends Enum<T>> T getMessageType(
            Class<T> enumType, String rawType, T defaultType) throws IOException {
        try {
            return Enum.valueOf(enumType, rawType);
        } catch (IllegalArgumentException e) {
            if (defaultType != null)
                return defaultType;
            throw new IOException("Invalid message type " + rawType);
        }
    }

    public static void write(JsonWriter writer, Message<?> message) throws IOException {
        writer.beginObject();
        message.write(writer);
        writer.endObject();
    }

    public static ClientMessage readClientMessage(JsonReader reader) throws IOException {
        String rawType = "";
        boolean success = false;

        reader.beginObject();
        while (reader.hasNext()) {
            String name = reader.nextName();
            if ("type".equals(name)) {
                rawType = reader.nextString();
            }
        }
        reader.endObject();

        switch (getMessageType(ClientMessageType.class,
                               rawType,
                               ClientMessageType.UNKNOWN_REQUEST)) {
            case UNKNOWN_REQUEST:
                return new UnknownRequestMessage(rawType);
        }
        throw new IOException("Invalid message");
    }

    public static ServerMessage readServerMessage(JsonReader reader) throws IOException {
        String rawType = "";
        String rawRequestType = null;

        reader.beginObject();
        while (reader.hasNext()) {
            String name = reader.nextName();
            if ("type".equals(name)) {
                rawType = reader.nextString();
            } else if ("rawRequestType".equals(name)) {
                rawRequestType = reader.nextString();
            }
        }
        reader.endObject();

        switch (getMessageType(ServerMessageType.class, rawType, null)) {
            case ICE_EXCHANGE:
                return new IceExchangeMessage();
            case UNKNOWN_RESPONSE:
                return new UnknownResponseMessage(rawRequestType);
        }
        throw new IOException("Invalid message");
    }

    /**
     * Base class for client and server message handlers.
     */
    public abstract static class MessageHandler {
        protected abstract void readMessage(JsonReader reader) throws IOException;

        public boolean readMessage(byte[] bytes) throws InvalidFormatException {
            try {
                readMessage(new JsonReader(new InputStreamReader(new ByteArrayInputStream(bytes))));
                return true;
            } catch (IOException e) {
                throw new InvalidFormatException(e);
            }
        }
    }

    /**
     * Exception when parsing or handling message.
     */
    public static class InvalidFormatException extends IOException {
        public InvalidFormatException(IOException e) {
            super(e);
        }

        public InvalidFormatException(String message) {
            super(message);
        }
    }

    /**
     * Base class for handler of server messages (to be created on client).
     */
    public abstract static class ServerMessageHandler extends MessageHandler {
        @Override
        protected void readMessage(JsonReader reader) throws IOException {
            onMessage(readServerMessage(reader));
        }

        protected abstract void onMessage(ServerMessage message);
    }

    /**
     * Base class for handler of client messages (to be created on server).
     */
    public abstract static class ClientMessageHandler extends MessageHandler {
        @Override
        public void readMessage(JsonReader reader) throws IOException {
            onMessage(readClientMessage(reader));
        }

        protected abstract void onMessage(ClientMessage message);
    }

    public static byte[] toByteArray(Message<?> message) {
        ByteArrayOutputStream byteStream = new ByteArrayOutputStream();
        JsonWriter writer = new JsonWriter(new OutputStreamWriter(byteStream));
        try {
            write(writer, message);
            writer.close();
            return byteStream.toByteArray();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
}
