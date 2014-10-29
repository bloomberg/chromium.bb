// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.commands;

import android.util.JsonReader;
import android.util.JsonWriter;

import org.chromium.components.devtools_bridge.RTCConfiguration;

import java.io.IOException;
import java.io.StringReader;
import java.io.StringWriter;
import java.util.ArrayList;
import java.util.List;

/**
 * Hepler class with a collection of parameter definitions.
 */
final class ParamDefinitions {
    public static ParamDefinition<String> newStringParam(String name) {
        return new ParamDefinition<String>(name) {
            @Override
            protected String fromString(String value) {
                return value;
            }

            @Override
            protected String toString(String value) {
                return value;
            }
        };
    }

    public static ParamDefinition<List<String>> newStringListParam(String name) {
        return new ParamDefinition<List<String>>(name) {
            @Override
            protected List<String> fromString(String value) throws CommandFormatException {
                try {
                    return new ParamReader(value)
                            .readStringList()
                            .close()
                            .stringListResult;
                } catch (Exception e) {
                    throw newException(this, "Expected JSON-serialized string list", e);
                }
            }

            @Override
            protected String toString(List<String> value) {
                try {
                    return new ParamWriter().write(value).close().toString();
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
            }
        };
    }

    public static ParamDefinition<RTCConfiguration> newConfigParam(String name) {
        return new ParamDefinition<RTCConfiguration>(name) {
            @Override
            protected RTCConfiguration fromString(String value) throws CommandFormatException {
                try {
                    return new ParamReader(value)
                        .readConfig()
                        .close()
                        .configResult;
                } catch (Exception e) {
                    throw newException(this, "Expected WebRTC configuration", e);
                }
            }

            @Override
            protected String toString(RTCConfiguration value) {
                try {
                    return new ParamWriter().write(value).close().toString();
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
            }
        };
    }

    private static CommandFormatException newException(
            ParamDefinition<?> param, String message, Exception cause) {
        return new CommandFormatException(
            "Exception in parameter " + param.name() + ": " + message, cause);
    }

    private static class ParamReader {
        private final JsonReader mReader;

        public List<String> stringListResult;
        public RTCConfiguration configResult;

        public ParamReader(String source) {
            mReader = new JsonReader(new StringReader(source));
        }

        public ParamReader readStringList() throws IOException {
            stringListResult = new ArrayList<String>();
            mReader.beginArray();
            while (mReader.hasNext()) {
                stringListResult.add(mReader.nextString());
            }
            mReader.endArray();
            return this;
        }

        public ParamReader readConfig() throws IOException {
            RTCConfiguration.Builder builder = new RTCConfiguration.Builder();
            mReader.beginObject();
            while (mReader.hasNext()) {
                String name = mReader.nextName();
                if ("iceServers".equals(name)) {
                    readIceServerList(builder);
                } else {
                    mReader.skipValue();
                }
            }
            configResult = builder.build();
            return this;
        }

        public ParamReader close() throws IOException {
            mReader.close();
            return this;
        }

        private void readIceServerList(RTCConfiguration.Builder builder) throws IOException {
            mReader.beginArray();
            while (mReader.hasNext()) {
                readIceServer(builder);
            }
            mReader.endArray();
        }

        private void readIceServer(RTCConfiguration.Builder builder) throws IOException {
            String uri = null;
            String username = "";
            String credential = "";
            mReader.beginObject();
            while (mReader.hasNext()) {
                String name = mReader.nextName();
                if ("uri".equals(name)) {
                    uri = mReader.nextString();
                } else if ("username".equals(name)) {
                    username = mReader.nextString();
                } else if ("credential".equals(name)) {
                    credential = mReader.nextString();
                } else {
                    mReader.skipValue();
                }
            }
            mReader.endObject();
            if (uri != null) {
                builder.addIceServer(uri, username, credential);
            }
        }
    }

    private static class ParamWriter {
        private final StringWriter mStringWriter = new StringWriter();
        private final JsonWriter mWriter = new JsonWriter(mStringWriter);

        public ParamWriter write(List<String> value) throws IOException {
            mWriter.beginArray();
            for (String item : value) {
                mWriter.value(item);
            }
            mWriter.endArray();
            return this;
        }

        public ParamWriter write(RTCConfiguration config) throws IOException {
            mWriter.beginObject();
            mWriter.name("iceServers");
            mWriter.beginArray();
            for (RTCConfiguration.IceServer server : config.iceServers) {
                mWriter.beginObject();
                mWriter.name("uri").value(server.uri);
                mWriter.name("username").value(server.username);
                mWriter.name("credential").value(server.credential);
                mWriter.endObject();
            }
            mWriter.endArray();
            mWriter.endObject();
            return this;
        }

        public ParamWriter close() throws IOException {
            mWriter.close();
            return this;
        }

        @Override
        public String toString() {
            return mStringWriter.toString();
        }
    }
}
