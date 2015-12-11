// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import android.app.PendingIntent.CanceledException;
import android.test.suitebuilder.annotation.LargeTest;
import android.view.View;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.media.remote.NotificationTransportControl.ListenerService;
import org.chromium.chrome.browser.media.remote.RemoteVideoInfo.PlayerState;
import org.chromium.chrome.test.util.ActivityUtils;

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
    public void testNotificationPause() throws InterruptedException, TimeoutException {
        castDefaultVideoFromPage(DEFAULT_VIDEO_PAGE);

        // Get the notification
        NotificationTransportControl notificationTransportControl = waitForCastNotification();
        assertNotNull("No notificationTransportControl", notificationTransportControl);
        // We can't actually click the notification's buttons, since they are owned by a different
        // process and hence is not accessible through instrumentation, so send the stop event
        // instead.
        NotificationTransportControl.ListenerService service =
                waitForCastNotificationService(notificationTransportControl);
        assertNotNull("No notification service", service);

        try {
            service.getPendingIntent(ListenerService.ACTION_ID_PAUSE).send();
        } catch (CanceledException e) {
            fail();
        }
        assertTrue("Not paused", waitForState(PlayerState.PAUSED));

        // The new position is sent in a separate message, so we have to wait a bit before
        // fetching it.
        Thread.sleep(STABILIZE_TIME_MS);
        int position = getRemotePositionMs();
        // Position should not change while paused
        Thread.sleep(PAUSE_TEST_TIME_MS);
        assertEquals("Pause didn't stop playback", position, getRemotePositionMs());
        try {
            service.getPendingIntent(ListenerService.ACTION_ID_PLAY).send();
        } catch (CanceledException e) {
            fail();
        }
        assertTrue("Not playing", waitForState(PlayerState.PLAYING));

        // Should now be running again.
        Thread.sleep(PAUSE_TEST_TIME_MS);
        assertTrue("Run didn't restart playback", position < getRemotePositionMs());
    }

    /**
     * Test select (pressing the body) on the notification.
     */
    @Feature({"VideoFling"})
    @LargeTest
    public void testNotificationSelect() throws InterruptedException, TimeoutException {
        castDefaultVideoFromPage(DEFAULT_VIDEO_PAGE);

        // Get the notification
        NotificationTransportControl notificationTransportControl = waitForCastNotification();
        assertNotNull("No notificationTransportControl", notificationTransportControl);
        // We can't actually click the notification's buttons, since they are owned by a different
        // process and hence is not accessible through instrumentation, so send the select event
        // instead.
        final NotificationTransportControl.ListenerService service =
                waitForCastNotificationService(notificationTransportControl);
        assertNotNull("No notification service", service);

        ExpandedControllerActivity fullscreenControls = ActivityUtils.waitForActivityWithTimeout(
                getInstrumentation(),
                ExpandedControllerActivity.class, new Runnable() {
                        @Override
                    public void run() {
                        try {
                            service.getPendingIntent(ListenerService.ACTION_ID_SELECT).send();
                        } catch (CanceledException e) {
                            fail();
                        }
                    }
                }, MAX_VIEW_TIME_MS);
        assertNotNull("No expanded controller activity", fullscreenControls);
        View rootView = fullscreenControls.findViewById(android.R.id.content);
        assertNotNull("No root view for fullscreen controls", rootView);
        assertEquals("Fullscreen controls not shown", View.VISIBLE, rootView.getVisibility());
    }
}
