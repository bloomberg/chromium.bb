// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.apiary;

import android.util.JsonReader;

import org.apache.http.client.HttpClient;

import org.chromium.components.devtools_bridge.gcd.RemoteInstance;
import org.chromium.components.devtools_bridge.gcd.TestMessageReader;

import java.io.IOException;
import java.util.List;

/**
 * Extension of GCDClient with methods needed for the testing app.
 */
public class TestGCDClient extends GCDClient {
    TestGCDClient(HttpClient httpClient, String apiKey, String oAuthToken) {
        super(httpClient, apiKey, oAuthToken);
        assert oAuthToken != null;
    }

    public List<RemoteInstance> fetchInstances() throws IOException {
        return mHttpClient.execute(
                newHttpGet("/devices"),
                new JsonResponseHandler<List<RemoteInstance>>() {
                    @Override
                    public List<RemoteInstance> readResponse(JsonReader reader)
                            throws IOException {
                        return new TestMessageReader(reader).readRemoteInstances();
                    }
                });
    }


}
