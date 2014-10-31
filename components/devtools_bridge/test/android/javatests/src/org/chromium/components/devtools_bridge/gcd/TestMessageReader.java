// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.gcd;

import android.util.JsonReader;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * Reader for additional messages (used only in the testing app).
 */
public final class TestMessageReader {
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
}
