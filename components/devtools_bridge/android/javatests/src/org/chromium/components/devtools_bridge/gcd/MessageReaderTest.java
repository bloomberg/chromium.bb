// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.gcd;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.Assert;

import org.chromium.components.devtools_bridge.commands.Command;
import org.chromium.components.devtools_bridge.util.TestSource;

/**
 * Tests for {@link MessageReader}.
 */
public class MessageReaderTest extends InstrumentationTestCase {
    private static final String DEVICE_ID = "4ac8a0f8-??????????????-192e2727710d";
    private static final String ROBOT_ACCOUNT_EMAIL =
            "2a3???????????????????????????87@clouddevices.gserviceaccount.com";
    private static final String AUTHORIZATION_CODE =
            "4/6V0jpup-????????????????????????????????????????????_e85kQI";
    private static final String COMMAND_ID =
            "a0217abb-????-????-????-?????????????????????????-????-2725-b2332fe99829";

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

    @SmallTest
    public void testReadNotificationCommandCreated() throws Exception {
        TestSource source = new TestSource();
        source.write().beginObject()
                .name("type").value("COMMAND_CREATED")
                .name("commandId").value(COMMAND_ID)
                .name("deviceId").value(DEVICE_ID)
                .name("command").beginObject()
                .name("kind").value("clouddevices#command")
                .name("id").value(COMMAND_ID)
                .name("deviceId").value(DEVICE_ID)
                .name("name").value("base._startSession")
                .name("parameters").beginObject()
                .name("_sessionId").value("SESSION_ID")
                .name("_config").value("{}")
                .name("_offer").value("INVALID_OFFER")
                .endObject() // parameters
                .name("state").value("queued")
                .name("error").beginObject()
                .name("arguments").beginArray().endArray()
                .name("creationTimeMs").value("1411137527329")
                .name("expirationTimeMs").value("1411137547329")
                .endObject() // error
                .endObject() // command
                .name("expirationTimeMs").value("1411029669794")
                .endObject().close();
        Notification result = new MessageReader(source.read()).readNotification();

        Assert.assertEquals(Notification.Type.COMMAND_CREATED, result.type);
        Assert.assertEquals(DEVICE_ID, result.instanceId);
        Assert.assertNotNull(result.command);
        Assert.assertEquals(COMMAND_ID, result.command.id);
        Assert.assertEquals(Command.Type.START_SESSION, result.command.type);
    }

    @SmallTest
    public void testReadNotificationCommandExpired() throws Exception {
        TestSource source = new TestSource();
        source.write().beginObject()
                .name("type").value("COMMAND_EXPIRED")
                .name("commandId").value(COMMAND_ID)
                .name("deviceId").value(DEVICE_ID)
                .endObject().close();
        Notification result = new MessageReader(source.read()).readNotification();
        Assert.assertNull(result);
    }

    @SmallTest
    public void testReadNotificationInstanceUNregistered() throws Exception {
        TestSource source = new TestSource();
        source.write().beginObject()
                .name("type").value("DEVICE_DELETED")
                .name("deviceId").value(DEVICE_ID)
                .endObject().close();
        Notification result = new MessageReader(source.read()).readNotification();

        Assert.assertEquals(Notification.Type.INSTANCE_UNREGISTERED, result.type);
        Assert.assertEquals(DEVICE_ID, result.instanceId);
        Assert.assertNull(result.command);
    }
}
