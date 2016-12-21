// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.graphics.Rect;
import android.support.test.filters.LargeTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.tab.Tab;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for the fullscreen cast controls.
 */
public class CastVideoControlsTest extends CastTestBase {

    private static final long PAUSE_TEST_TIME_MS = 1000;

    /*
     * Test the pause button.
     */
    @Feature({"VideoFling"})
    @LargeTest
    @RetryOnFailure
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE) // crbug.com/652872
    public void testPauseButton() throws InterruptedException, TimeoutException {
        Rect videoRect = castDefaultVideoFromPage(DEFAULT_VIDEO_PAGE);

        final Tab tab = getActivity().getActivityTab();

        tapPlayPauseButton(tab, videoRect);
        // The new position is sent in a separate message, so we have to wait a bit before
        // fetching it.
        long position = getRemotePositionMs();
        boolean paused = false;
        for (int time = 0; time < MAX_VIEW_TIME_MS; time += VIEW_RETRY_MS) {
            Thread.sleep(VIEW_RETRY_MS);
            long newPosition = getRemotePositionMs();
            if (newPosition == position) {
                paused = true;
                break;
            }
            position = newPosition;
        }
        // Check we have paused before the end of the video (with a fudge factor for timing
        // variation)
        assertTrue("Pause didn't stop playback", paused || position < getRemoteDurationMs() - 100);
        tapPlayPauseButton(tab, videoRect);
        Thread.sleep(PAUSE_TEST_TIME_MS);
        assertTrue("Run didn't restart playback", position < getRemotePositionMs());
    }
}
