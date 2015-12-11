// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import android.app.PendingIntent.CanceledException;
import android.graphics.Rect;
import android.test.suitebuilder.annotation.LargeTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.media.remote.NotificationTransportControl.ListenerService;
import org.chromium.chrome.browser.tab.Tab;

import java.util.concurrent.TimeoutException;

/**
 * Simple tests of casting videos. These tests all use the same page, containing the same non-YT
 * video.
 */
public class CastStartStopTest extends CastTestBase {

    /*
     * Test that we can cast a video, and that we get the ExpandedControllerActivity when we do.
     */
    @Feature({"VideoFling"})
    @LargeTest
    public void testCastingGenericVideo() throws InterruptedException, TimeoutException {
        castDefaultVideoFromPage(DEFAULT_VIDEO_PAGE);
        checkVideoStarted(DEFAULT_VIDEO);
    }

    /*
     * Test that we can disconnect a cast session from the expanded controller activity overlay.
     */
    @Feature({"VideoFling"})
    @LargeTest
    public void testStopFromVideoControls() throws InterruptedException, TimeoutException {
        Rect videoRect = castDefaultVideoFromPage(DEFAULT_VIDEO_PAGE);

        final Tab tab = getActivity().getActivityTab();

        clickDisconnectFromRoute(tab, videoRect);

        checkDisconnected();
    }

    /*
     * Test that we can stop a cast session from the notification.
     */
    @Feature({"VideoFling"})
    @LargeTest
    public void testStopFromNotification() throws InterruptedException, TimeoutException {
        castDefaultVideoFromPage(DEFAULT_VIDEO_PAGE);

        // Get the notification
        NotificationTransportControl notificationTransportControl = waitForCastNotification();

        // We can't actually click the notification's stop button, since it is owned by a different
        // process and hence is not accessible through instrumentation, so send the stop event
        // instead.
        NotificationTransportControl.ListenerService service =
                waitForCastNotificationService(notificationTransportControl);

        try {
            service.getPendingIntent(ListenerService.ACTION_ID_STOP).send();
        } catch (CanceledException e) {
            fail();
        }
        checkDisconnected();
    }

    /*
     * Test that a cast session disconnects when the video ends
     */
    @Feature({"VideoFling"})
    @LargeTest
    public void testStopWhenVideoEnds() throws InterruptedException, TimeoutException {
        castDefaultVideoFromPage(DEFAULT_VIDEO_PAGE);
        // Wait for the video to finish (this assumes the video is short, the test video
        // is 8 seconds).
        sleepNoThrow(STABILIZE_TIME_MS);

        Thread.sleep(getRemoteDurationMs());

        // Everything should now have disconnected
        checkDisconnected();
    }
}
