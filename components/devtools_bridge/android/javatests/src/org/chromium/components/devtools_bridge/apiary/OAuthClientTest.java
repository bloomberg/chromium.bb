// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.apiary;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.Assert;

import org.chromium.components.devtools_bridge.util.TestSource;

/**
 * Tests for {@link OAuthClient}.
 */
public class OAuthClientTest extends InstrumentationTestCase {
    private static final String ACCESS_TOKEN =
            "ya29.rgBgy64Y1MACXNmPDUpPGbwFuAec2NCCDJwaEp8DwLnV8RBk45p9RBqBfEQUYxL6OVB-oyktRqZj0w";
    private static final String REFRESH_TOKEN =
            "1/cWihsJmDMujYfhzBVTwgh4ukiFyiiRWLmFwTv4EigzU";

    @SmallTest
    public void testResponse() throws Exception {
        TestSource source = new TestSource();
        source.write()
                .beginObject()
                .name("access_token").value(ACCESS_TOKEN)
                .name("token_type").value("Bearer")
                .name("expires_in").value(3600) // seconds
                .name("refresh_token").value(REFRESH_TOKEN)
                .endObject()
                .close();
        OAuthResult result = OAuthClient.readResponse(source.read(), 1111);
        Assert.assertEquals(ACCESS_TOKEN, result.accessToken);
        Assert.assertEquals(REFRESH_TOKEN, result.refreshToken);
        Assert.assertEquals(1111 + 3600000, result.expirationTimeMs);
    }
}
