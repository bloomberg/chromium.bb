// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Notification;

import org.chromium.base.test.util.Feature;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import java.util.Arrays;

/**
 * Unit tests for NotificationPlatformBridge.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class NotificationPlatformBridgeTest {
    /**
     * Verifies that the getOriginFromTag method returns the origin for valid input, and null for
     * invalid input.
     */
    @Test
    @Feature({"Browser", "Notifications"})
    public void testGetOriginFromTag() throws Exception {
        // The common case.
        assertEquals(
                "https://example.com", NotificationPlatformBridge.getOriginFromTag(
                                               "NotificationPlatformBridge;https://example.com;42"));

        // An tag that includes the separator. Probably a bit unusual, but valid.
        assertEquals("https://example.com", NotificationPlatformBridge.getOriginFromTag(
                "NotificationPlatformBridge;https://example.com;this;tag;contains;the;separator"));

        // Some invalid input.
        assertNull(NotificationPlatformBridge.getOriginFromTag("SystemDownloadNotifier"));
        assertNull(NotificationPlatformBridge.getOriginFromTag(null));
        assertNull(NotificationPlatformBridge.getOriginFromTag(""));
        assertNull(NotificationPlatformBridge.getOriginFromTag(";"));
        assertNull(NotificationPlatformBridge.getOriginFromTag(";;;;;;;"));
        assertNull(NotificationPlatformBridge.getOriginFromTag(
                "SystemDownloadNotifier;NotificationPlatformBridge;42"));
        assertNull(NotificationPlatformBridge.getOriginFromTag(
                "SystemDownloadNotifier;https://example.com;42"));
        assertNull(NotificationPlatformBridge.getOriginFromTag(
                "NotificationPlatformBridge;SystemDownloadNotifier;42"));
    }

    /**
     * Verifies that the makeDefaults method returns the generated notification defaults.
     */
    @Test
    @Feature({"Browser", "Notifications"})
    public void testMakeDefaults() throws Exception {
        // 0 should be returned if pattern length is 0, silent is true, and vibration is enabled.
        assertEquals(0, NotificationPlatformBridge.makeDefaults(0, true, true));

        // Notification.DEFAULT_ALL should be returned if pattern length is 0, silent is false and
        // vibration is enabled.
        assertEquals(
                Notification.DEFAULT_ALL, NotificationPlatformBridge.makeDefaults(0, false, true));

        // Vibration should be removed from the defaults if pattern length is greater than 0, silent
        // is false, and vibration is enabled.
        assertEquals(Notification.DEFAULT_ALL & ~Notification.DEFAULT_VIBRATE,
                NotificationPlatformBridge.makeDefaults(10, false, true));

        // Vibration should be removed from the defaults if pattern length is greater than 0, silent
        // is false, and vibration is disabled.
        assertEquals(Notification.DEFAULT_ALL & ~Notification.DEFAULT_VIBRATE,
                NotificationPlatformBridge.makeDefaults(7, false, false));
    }

    /**
     * Verifies that the makeVibrationPattern method returns vibration pattern used
     * in Android notification.
     */
    @Test
    @Feature({"Browser", "Notifications"})
    public void testMakeVibrationPattern() throws Exception {
        assertTrue(Arrays.equals(new long[] {0, 100, 200, 300},
                NotificationPlatformBridge.makeVibrationPattern(new int[] {100, 200, 300})));
    }
}
