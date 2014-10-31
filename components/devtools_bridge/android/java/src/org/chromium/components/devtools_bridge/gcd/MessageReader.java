// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.gcd;

import android.util.JsonReader;

import java.io.IOException;

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
}
