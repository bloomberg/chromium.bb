// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import android.os.Bundle;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for GCMDriver.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GCMDriverTest {
    /**
     * Tests that GCM messages are persisted and read.
     */
    @Test
    public void testReadingPersistedMessage() {
        final String subscriptionId = "subscriptionId";
        final String anotherSubscriptionId = "AnotherSubscriptionId";

        Bundle extras = new Bundle();
        extras.putString("subtype", "MyAppId");
        extras.putString("collapse_key", "CollapseKey");
        GCMMessage message = new GCMMessage("MySenderId", extras);
        GCMDriver.persistMessage(subscriptionId, message);

        GCMMessage messages[] = GCMDriver.readMessages(subscriptionId);
        assertEquals(1, messages.length);
        assertEquals(message.getSenderId(), messages[0].getSenderId());

        messages = GCMDriver.readMessages(anotherSubscriptionId);
        assertEquals(0, messages.length);
    }

    /**
     * Tests that only MESSAGES_QUEUE_SIZE messages are kept.
     */
    @Test
    public void testPersistingMessageCount() {
        // This tests persists MESSAGES_QUEUE_SIZE+extraMessagesCount messages
        // and checks if only the most recent |MESSAGES_QUEUE_SIZE| are read.
        // |collapse_key| is used to distinguish between messages for
        // simplicity.
        final String subscriptionId = "subscriptionId";
        final String collapseKeyPrefix = "subscriptionId";
        final int extraMessagesCount = 5;

        // Persist |MESSAGES_QUEUE_SIZE| + |extraMessagesCount| messages.
        for (int i = 0; i < GCMDriver.MESSAGES_QUEUE_SIZE + extraMessagesCount; i++) {
            Bundle extras = new Bundle();
            extras.putString("subtype", "MyAppId");
            extras.putString("collapse_key", collapseKeyPrefix + i);
            GCMMessage message = new GCMMessage("MySenderId", extras);
            GCMDriver.persistMessage(subscriptionId, message);
        }
        // Check that only the most recent |MESSAGES_QUEUE_SIZE| are persisted.
        GCMMessage messages[] = GCMDriver.readMessages(subscriptionId);
        assertEquals(GCMDriver.MESSAGES_QUEUE_SIZE, messages.length);
        for (int i = 0; i < GCMDriver.MESSAGES_QUEUE_SIZE; i++) {
            assertEquals(
                    collapseKeyPrefix + (i + extraMessagesCount), messages[i].getCollapseKey());
        }
    }

    /**
     * Tests that messages with the same collapse key override each other.
     */
    @Test
    public void testCollapseKeyCollision() {
        final String subscriptionId = "subscriptionId";
        final String collapseKey = "collapseKey";
        final byte[] rawData1 = {0x00, 0x15, 0x30, 0x01};
        final byte[] rawData2 = {0x00, 0x15, 0x30, 0x02};

        Bundle extras = new Bundle();
        extras.putString("subtype", "MyAppId");
        extras.putString("collapse_key", collapseKey);
        extras.putByteArray("rawData", rawData1);

        // Persist a message and make sure it's persisted.
        GCMMessage message1 = new GCMMessage("MySenderId", extras);
        GCMDriver.persistMessage(subscriptionId, message1);

        GCMMessage messages[] = GCMDriver.readMessages(subscriptionId);
        assertEquals(1, messages.length);
        assertArrayEquals(rawData1, messages[0].getRawData());

        // Persist another message with the same collapse key and another raw data.
        extras.putByteArray("rawData", rawData2);
        GCMMessage message2 = new GCMMessage("MySenderId", extras);
        GCMDriver.persistMessage(subscriptionId, message2);

        messages = GCMDriver.readMessages(subscriptionId);
        assertEquals(1, messages.length);
        assertArrayEquals(rawData2, messages[0].getRawData());
    }
}
