// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.apiary;

import android.util.JsonReader;

import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpGet;

import org.chromium.components.devtools_bridge.RTCConfiguration;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * Client for fetching TURN configuration form a demo server.
 */
public final class TurnConfigClient {
    private static final String URL =
            "http://computeengineondemand.appspot.com/turn?username=28230128&key=4080218913";
    private static final String STUN_URL = "stun.l.google.com:19302";

    private final HttpClient mHttpClient;

    TurnConfigClient(HttpClient httpClient) {
        mHttpClient = httpClient;
    }

    public RTCConfiguration fetch() throws IOException {
        return mHttpClient.execute(
                new HttpGet(URL), new ConfigResponseHandler());
    }

    private class ConfigResponseHandler extends JsonResponseHandler<RTCConfiguration> {
        public RTCConfiguration readResponse(JsonReader reader) throws IOException {
            List<String> uris = null;
            String username = null;
            String password = null;

            reader.beginObject();
            while (reader.hasNext()) {
                String name = reader.nextName();
                if ("username".equals(name)) {
                    username = reader.nextString();
                } else if ("password".equals(name)) {
                    password = reader.nextString();
                } else if ("uris".equals(name)) {
                    uris = readStringList(reader);
                } else {
                    reader.skipValue();
                }
            }
            reader.endObject();

            RTCConfiguration.Builder builder = new RTCConfiguration.Builder();
            builder.addIceServer(STUN_URL);
            for (String uri : uris) {
                builder.addIceServer(uri, username, password);
            }
            return builder.build();
        }
    }

    private List<String> readStringList(JsonReader reader) throws IOException {
        ArrayList<String> result = new ArrayList<String>();
        reader.beginArray();
        while (reader.hasNext()) {
            result.add(reader.nextString());
        }
        reader.endArray();
        return result;
    }
}
