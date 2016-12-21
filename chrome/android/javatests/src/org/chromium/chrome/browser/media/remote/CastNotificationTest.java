// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.support.test.filters.LargeTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.media.remote.RemoteVideoInfo.PlayerState;
import org.chromium.chrome.browser.media.ui.MediaNotificationListener;

import java.util.concurrent.TimeoutException;

/**
 * Tests of the notification.
 */
public class CastNotificationTest extends CastTestBase {

    private static final long PAUSE_TEST_TIME_MS = 1000;

    /**
     * Test the pause button on the notification.
     */
    @Feature({"VideoFling"})
    @LargeTest
    @RetryOnFailure
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE) // crbug.com/652872
    public void testNotificationPause() throws InterruptedException, TimeoutException {
        castDefaultVideoFromPage(DEFAULT_VIDEO_PAGE);

        // Get the notification
        final CastNotificationControl notificationControl = waitForCastNotification();
        assertNotNull("No notificationTransportControl", notificationControl);
        // Send pause
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                notificationControl.onPause(MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
            }
        });
        assertTrue("Not paused", waitForState(PlayerState.PAUSED, MAX_VIEW_TIME_MS));

        // The new position is sent in a separate message, so we have to wait a bit before
        // fetching it.
        Thread.sleep(STABILIZE_TIME_MS);
        long position = getRemotePositionMs();
        // Position should not change while paused
        Thread.sleep(PAUSE_TEST_TIME_MS);
        assertEquals("Pause didn't stop playback", position, getRemotePositionMs());
        // Send play
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                notificationControl.onPlay(MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
            }
        });
        assertTrue("Not playing", waitForState(PlayerState.PLAYING, MAX_VIEW_TIME_MS));

        // Should now be running again.
        Thread.sleep(PAUSE_TEST_TIME_MS);
        assertTrue("Run didn't restart playback", position < getRemotePositionMs());
    }
}
