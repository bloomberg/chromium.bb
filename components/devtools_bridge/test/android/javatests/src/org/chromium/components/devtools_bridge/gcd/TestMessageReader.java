// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.gcd;

import android.util.JsonReader;
import android.util.Log;

import org.chromium.components.devtools_bridge.commands.Command;
import org.chromium.components.devtools_bridge.commands.CommandFormatException;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Reader for additional messages (used only in the testing app).
 */
public final class TestMessageReader {
    private static final String TAG = "TestMessageReader";

    private final JsonReader mReader;

    public TestMessageReader(JsonReader reader) {
        mReader = reader;
    }

    public List<RemoteInstance> readRemoteInstances() throws IOException {
        final List<RemoteInstance> result = new ArrayList<RemoteInstance>();

        mReader.beginObject();
        while (mReader.hasNext()) {
            String name = mReader.nextName();
            if (name.equals("devices")) {
                mReader.beginArray();
                while (mReader.hasNext()) {
                    result.add(readInstance());
                }
                mReader.endArray();
            } else {
                mReader.skipValue();
            }
        }
        mReader.endObject();
        return result;
    }

    private RemoteInstance readInstance() throws IOException {
        String id = null;
        String displayName = null;

        mReader.beginObject();
        while (mReader.hasNext()) {
            String name = mReader.nextName();
            if (name.equals("id")) {
                id = mReader.nextString();
            } else if (name.equals("displayName")) {
                displayName = mReader.nextString();
            } else {
                mReader.skipValue();
            }
        }
        mReader.endObject();
        if (id == null) {
            throw new IllegalArgumentException("Missing remote instance id");
        }
        if (displayName == null) {
            throw new IllegalArgumentException("Missing remote instance display name");
        }
        return new RemoteInstance(id, displayName);
    }

    public void readCommandResult(Command command) throws IOException {
        String state = null;
        Map<String, String> outParams = null;
        String errorMessage = null;

        mReader.beginObject();
        while (mReader.hasNext()) {
            String name = mReader.nextName();
            if (name.equals("state")) {
                state = mReader.nextString();
            } else if (name.equals("results")) {
                outParams = MessageReader.readStringMap(mReader);
            } else if (name.equals("error")) {
                errorMessage = readErrorMessage();
            } else {
                mReader.skipValue();
            }
        }
        mReader.endObject();

        if ("done".equals(state) && outParams != null) {
            try {
                command.setSuccess(outParams);
            } catch (CommandFormatException e) {
                Log.e(TAG, "Invalid command format", e);
                command.setFailure("Invalid format: " + e.getMessage());
            }
        } else if ("error".equals(state) && errorMessage != null) {
            Log.w(TAG, "Command error: " + errorMessage);
            command.setFailure(errorMessage);
        } else {
            Log.w(TAG, "Invalid command state: " + state);
            command.setFailure("Invalid state: " + state);
        }
    }

    private String readErrorMessage() throws IOException {
        String result = null;
        mReader.beginObject();
        while (mReader.hasNext()) {
            if (mReader.nextName().equals("message")) {
                result = mReader.nextString();
            } else {
                mReader.skipValue();
            }
        }
        mReader.endObject();
        return result;
    }
}
