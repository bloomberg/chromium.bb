// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.gcd;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.Assert;

import org.chromium.components.devtools_bridge.util.TestSource;

/**
 * Tests for {@link MessageReaderTest}.
 */
public class MessageReaderTest extends InstrumentationTestCase {
    private static final String DEVICE_ID = "4ac8a0f8-??????????????-192e2727710d";
    private static final String ROBOT_ACCOUNT_EMAIL =
            "2a3???????????????????????????87@clouddevices.gserviceaccount.com";
    private static final String AUTHORIZATION_CODE =
            "4/6V0jpup-????????????????????????????????????????????_e85kQI";

    @SmallTest
    public void testReadTicket() throws Exception {
        TestSource source = new TestSource();
        source.write().beginObject()
                .name("kind").value("clouddevices#registrationTicket")
                .name("id").value("p8hI4")
                .name("deviceId").value(DEVICE_ID)
                .name("creationTimeMs").value("1411029429794")
                .name("expirationTimeMs").value("1411029669794")
                .endObject().close();
        String result = new MessageReader(source.read()).readTicketId();
        Assert.assertEquals("p8hI4", result);
    }

    @SmallTest
    public void testReadCredential() throws Exception {
        TestSource source = new TestSource();
        source.write().beginObject()
                .name("kind").value("clouddevices#registrationTicket")
                .name("id").value("p8hI4")
                .name("deviceId").value(DEVICE_ID)
                .name("userEmail").value("...@chromium.org")
                .name("creationTimeMs").value("1411029429794")
                .name("expirationTimeMs").value("1411029669794")
                .name("robotAccountEmail").value(ROBOT_ACCOUNT_EMAIL)
                .name("robotAccountAuthorizationCode").value(AUTHORIZATION_CODE)
                .endObject().close();
        InstanceCredential result = new MessageReader(source.read()).readInstanceCredential();
        Assert.assertEquals(DEVICE_ID, result.id);
        Assert.assertEquals(AUTHORIZATION_CODE, result.secret);
    }
}
