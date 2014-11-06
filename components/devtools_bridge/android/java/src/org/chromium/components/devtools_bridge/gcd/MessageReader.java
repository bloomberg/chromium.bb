// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.gcd;

import android.util.JsonReader;

import org.chromium.components.devtools_bridge.commands.Command;
import org.chromium.components.devtools_bridge.commands.CommandFormatException;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/**
 * Helper class for parsing JSON-encoded GCD messages (HTTP responses and GCM notifications) used
 * in the DevTools bridge.
 */
public final class MessageReader {
    private final JsonReader mReader;

    public MessageReader(JsonReader reader) {
        mReader = reader;
    }

    /**
     * Reads id from a registration ticket.
     */
    public String readTicketId() throws IOException {
        return new TicketReader().readId();
    }

    /**
     * Reads credentials from finalized registration ticket.
     */
    public InstanceCredential readInstanceCredential() throws IOException {
        return new TicketReader().readCredential();
    }

    Notification readNotification() throws IOException, CommandFormatException {
        return new NotificationReader().read();
    }

    private abstract class ObjectReader {
        public final void readObject() throws IOException {
            mReader.beginObject();
            while (mReader.hasNext()) {
                readItem(mReader.nextName());
            }
            mReader.endObject();
        }

        protected void readItem(String name) throws IOException {
            mReader.skipValue();
        }
    }

    private class TicketReader extends ObjectReader {
        private String mId;
        private String mDeviceId;
        private String mDeviceSecret;

        public String readId() throws IOException {
            readObject();
            if (mId == null) {
                throw new IllegalArgumentException();
            }
            return mId;
        }

        public InstanceCredential readCredential() throws IOException {
            readObject();
            if (mDeviceId == null || mDeviceSecret == null) {
                throw new IllegalArgumentException();
            }
            return new InstanceCredential(mDeviceId, mDeviceSecret);
        }

        @Override
        protected void readItem(String name) throws IOException {
            if (name.equals("id")) {
                mId = mReader.nextString();
            } else if (name.equals("deviceId")) {
                mDeviceId = mReader.nextString();
            } else if (name.equals("robotAccountAuthorizationCode")) {
                mDeviceSecret = mReader.nextString();
            } else {
                super.readItem(name);
            }
        }
    }

    private class NotificationReader extends ObjectReader {
        private String mDeviceId;
        private String mType;
        private String mCommandId;
        private CommandReader mCommandReader;

        public Notification read() throws IOException, CommandFormatException {
            readObject();
            if (mDeviceId == null || mType == null) return null;
            if (mType.equals("COMMAND_CREATED")) {
                if (mCommandReader == null) {
                    throw new CommandFormatException("Command missing");
                }
                if (mCommandId == null) {
                    throw new CommandFormatException("Command id missing");
                }
                return new Notification(
                        mDeviceId, Notification.Type.COMMAND_CREATED,
                        mCommandReader.newCommand(mCommandId));
            } else if (mType.equals("DEVICE_DELETED")) {
                return new Notification(mDeviceId, Notification.Type.INSTANCE_UNREGISTERED, null);
            } else {
                return null;
            }
        }

        @Override
        protected void readItem(String name) throws IOException {
            if (name.equals("deviceId")) {
                mDeviceId = mReader.nextString();
            } else if (name.equals("type")) {
                mType = mReader.nextString();
            } else if (name.equals("commandId")) {
                mCommandId = mReader.nextString();
            } else if (name.equals("command")) {
                mCommandReader = new CommandReader();
                mCommandReader.readObject();
            } else {
                super.readItem(name);
            }
        }
    }

    private class CommandReader extends ObjectReader {
        private String mType;
        private Map<String, String> mParameters;

        public Command newCommand(String id) throws CommandFormatException {
            if (mType == null) {
                throw new CommandFormatException("Missing command type");
            }
            if (mParameters == null) {
                throw new CommandFormatException("Missing parameters");
            }
            return convertType(mType).definition.newCommand(id, mParameters);
        }

        @Override
        protected void readItem(String name) throws IOException {
            if (name.equals("name")) {
                mType = mReader.nextString();
            } else if (name.equals("parameters")) {
                mParameters = readStringMap(mReader);
            } else {
                super.readItem(name);
            }
        }

        private Command.Type convertType(String name) throws CommandFormatException {
            for (Command.Type type : Command.Type.values()) {
                if (type.definition.fullName().equals(name)) return type;
            }
            throw new CommandFormatException("Invalid type: " + name);
        }
    }

    static Map<String, String> readStringMap(JsonReader reader) throws IOException {
        Map<String, String> result = new HashMap<String, String>();
        reader.beginObject();
        while (reader.hasNext()) {
            String name = reader.nextName();
            String value = reader.nextString();
            result.put(name, value);
        }
        reader.endObject();
        return result;
    }
}
