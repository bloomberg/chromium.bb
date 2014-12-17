// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.apiary;

/**
 * Implementation of ApiaryClientFactory for manual testing.
 */
public class TestApiaryClientFactory extends ApiaryClientFactory {
    public TestGCDClient newTestGCDClient(String oAuthToken) {
        return new TestGCDClient(mHttpClient, getAPIKey(), oAuthToken);
    }

    public TurnConfigClient newConfigClient() {
        return new TurnConfigClient(mHttpClient);
    }
}
