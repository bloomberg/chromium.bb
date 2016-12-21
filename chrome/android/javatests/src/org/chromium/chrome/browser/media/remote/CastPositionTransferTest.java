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
import org.chromium.content.browser.test.util.JavaScriptUtils;

import java.util.Date;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests related to the transfer of the playback position between the local and
 * the remote player.
 */
@RetryOnFailure
public class CastPositionTransferTest extends CastTestBase {

    /** Reference position in the video where we should start casting */
    private static final int CAST_START_TIME_MS = 3000;

    /** Max accepted error for position comparisons. */
    private static final int SEEK_EPSILON_MS = 250;

    /** Used to wait for the UI to properly respond. It is smaller than the default
     * {@link CastTestBase#STABILIZE_TIME_MS} to make sure the video doesn't finish while waiting.*/
    private static final int SMALL_STABILIZE_TIME_MS = 250;


    /** Returns the current time of the video, in milliseconds,  by getting it from JavaScript. */
    private static long getLocalPositionMillis(Tab tab, String videoElementId)
            throws InterruptedException, TimeoutException, NumberFormatException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var node = document.getElementById('" + videoElementId + "');");
        sb.append("  if (node) return node.currentTime;");
        sb.append("  return null");
        sb.append("})();");
        String jsResult = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), sb.toString());
        return Math.round(Double.parseDouble(jsResult) * 1000);
    }

    private static void seekFromJS(Tab tab, long seekToMs)
            throws InterruptedException, TimeoutException, NumberFormatException {
        final long seekToSec = TimeUnit.MILLISECONDS.toSeconds(seekToMs);
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var node = document.getElementById('" + VIDEO_ELEMENT + "');");
        sb.append("  if (node) node.currentTime = " + seekToSec + ";");
        sb.append("  return 0;");
        sb.append("})();");
        JavaScriptUtils.executeJavaScriptAndWaitForResult(tab.getWebContents(), sb.toString());
    }

    /** Test for crbug.com/428409 */
    @Feature({"VideoFling"})
    @LargeTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE) // crbug.com/652872
    public void testLocalToRemotePositionTransfer() throws InterruptedException, TimeoutException {
        final Tab tab = getActivity().getActivityTab();
        final Rect videoRect = prepareDefaultVideofromPage(DEFAULT_VIDEO_PAGE, tab);

        // Jump to the position
        seekFromJS(tab, CAST_START_TIME_MS);
        final double localPositionMs = getLocalPositionMillis(tab, VIDEO_ELEMENT);
        assertTrue("Local playback position (" + localPositionMs + ") did not advance past "
                + CAST_START_TIME_MS, localPositionMs >= CAST_START_TIME_MS);

        // Start cast
        final long castStartTimeMs = new Date().getTime();
        castVideoAndWaitUntilPlaying(CAST_TEST_ROUTE, tab, videoRect);

        // Test the position
        final long remotePositionMs = getRemotePositionMs();
        final long castDelayMs = new Date().getTime() - castStartTimeMs;

        assertTrue("The remote playback position (" + remotePositionMs + ") did not advance past "
                + CAST_START_TIME_MS, remotePositionMs >= CAST_START_TIME_MS);
        assertTrue("The remote playback position (" + remotePositionMs + ") went too far.",
                remotePositionMs <= CAST_START_TIME_MS + castDelayMs);
    }

    /** Test for crbug.com/428409 */
    @Feature({"VideoFling"})
    @LargeTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE) // crbug.com/652872
    public void testRemoteToLocalPositionTransfer() throws InterruptedException, TimeoutException {
        final Tab tab = getActivity().getActivityTab();
        final Rect videoRect = prepareDefaultVideofromPage(DEFAULT_VIDEO_PAGE, tab);

        // Start cast
        castVideoAndWaitUntilPlaying(CAST_TEST_ROUTE, tab, videoRect);

        tapPlayPauseButton(tab, videoRect);
        long pausePosition = getRemotePositionMs();
        for (int time = 0; time < MAX_VIEW_TIME_MS; time += VIEW_RETRY_MS) {
            Thread.sleep(VIEW_RETRY_MS);
            long newPosition = getRemotePositionMs();
            if (newPosition == pausePosition) {
                break;
            }
            pausePosition = newPosition;
        }
        // Jump to the position
        seekFromJS(tab, CAST_START_TIME_MS);
        for (int time = 0; time < MAX_VIEW_TIME_MS
                && getRemotePositionMs() != pausePosition; time += VIEW_RETRY_MS) {
            Thread.sleep(VIEW_RETRY_MS);
        }
        long remotePositionMs = getRemotePositionMs();
        assertEquals("The remote player did not seek",
                CAST_START_TIME_MS, remotePositionMs, SEEK_EPSILON_MS);

        // Stop cast and play locally
        final long castStopTimeMs = new Date().getTime();
        clickDisconnectFromRoute(tab, videoRect);
        tapPlayPauseButton(tab, videoRect);
        sleepNoThrow(SMALL_STABILIZE_TIME_MS);

        // Test the position
        final double localPositionMs = getLocalPositionMillis(tab, VIDEO_ELEMENT);
        final long castDelayMs = new Date().getTime() - castStopTimeMs;
        assertTrue("The local playback position (" + localPositionMs + ") did not advance past "
                + CAST_START_TIME_MS, localPositionMs >= CAST_START_TIME_MS);
        assertTrue("The local playback position (" + localPositionMs + ") went too far.",
                localPositionMs <= CAST_START_TIME_MS + castDelayMs);
    }

    /** Test for crbug.com/425105 */
    @Feature({"VideoFling"})
    @LargeTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE) // crbug.com/593840, crbug.com/652872
    public void testPositionUpdate() throws InterruptedException, TimeoutException {
        final Tab tab = getActivity().getActivityTab();
        final Rect videoRect = prepareDefaultVideofromPage(DEFAULT_VIDEO_PAGE, tab);
        castVideoAndWaitUntilPlaying(CAST_TEST_ROUTE, tab, videoRect);

        sleepNoThrow(CAST_START_TIME_MS);

        final long remotePositionMs = getRemotePositionMs();
        final long localPositionMs = getLocalPositionMillis(tab, VIDEO_ELEMENT);

        assertEquals("Remote playback position was not properly updated.",
                CAST_START_TIME_MS, remotePositionMs, SEEK_EPSILON_MS);
        assertEquals("The remote playback position was updated, but the local one was not.",
                CAST_START_TIME_MS, localPositionMs, SEEK_EPSILON_MS);
    }
}
