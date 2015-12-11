// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.graphics.Rect;
import android.view.View;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.TimeoutException;

/**
 * Test casting when Chrome shuts down and restarts.
 *
 *  These tests are run from the python wrapper test/host_driven_tests/ReconnectCastTest.py. They
 * will not run correctly in isolation, and hence are marked as disabled to prevent the test runner
 * from running them automatically outside the wrapper.
 */
public class CastReconnectTest extends CastTestBase {

    private int mVideoDurationMs;

    /**
     * Cast and pause a video. This doesn't test anything itself, but just sets up the state for
     * when we restart Chrome
     *
     * @throws InterruptedException
     * @throws TimeoutException
     */
    @DisabledTest
    public void testReconnectSetup() throws InterruptedException, TimeoutException {
        castAndPauseDefaultVideoFromPage(DEFAULT_VIDEO_PAGE);
    }

    /**
     * Test the state after reconnection, and confirm that pressing play starts the video.
     *
     * @throws InterruptedException
     * @throws TimeoutException
     */
    @DisabledTest
    public void testVideoContinuing() throws InterruptedException, TimeoutException {
        // TODO(aberent) Check that the notification is still present. Will fail at
        // the moment, and is difficult to do with the way the notification service
        // currently works.

        // Put the video into fullscreen and check that it casts automatically
        final Tab tab = getActivity().getActivityTab();

        WebContents webContents = tab.getWebContents();
        waitUntilVideoReady(VIDEO_ELEMENT, webContents);

        final Rect videoRect = DOMUtils.getNodeBounds(webContents, VIDEO_ELEMENT);
        ExpandedControllerActivity fullscreenControls = ActivityUtils.waitForActivityWithTimeout(
                getInstrumentation(), ExpandedControllerActivity.class, new Runnable() {
                    @Override
                    public void run() {
                        tapVideoFullscreenButton(tab, videoRect);
                    }
                }, MAX_VIEW_TIME_MS);
        assertNotNull("Fullscreen controls not recreated", fullscreenControls);
        View pauseButton = fullscreenControls.findViewById(R.id.pause);
        assertNotNull("No pause/play button", pauseButton);
        clickButton(pauseButton);

        // Give it time to start
        Thread.sleep(STABILIZE_TIME_MS);

        checkVideoStarted(DEFAULT_VIDEO);
    }

    /**
     * Test going into fullscreen on a new video after reconnection.
     *
     * @throws InterruptedException
     * @throws TimeoutException
     */
    @DisabledTest
    public void testNewVideo() throws InterruptedException, TimeoutException {

        // Load a new video on the page
        loadUrl(TestHttpServerClient.getUrl(TEST_VIDEO_PAGE_2));

        // Put the video into fullscreen
        final Tab tab = getActivity().getActivityTab();

        WebContents webContents = tab.getWebContents();
        waitUntilVideoReady(VIDEO_ELEMENT, webContents);

        final Rect videoRect = DOMUtils.getNodeBounds(webContents, VIDEO_ELEMENT);

        // Video should cast automatically
        ExpandedControllerActivity fullscreenControls = ActivityUtils.waitForActivityWithTimeout(
                getInstrumentation(), ExpandedControllerActivity.class, new Runnable() {
                    @Override
                    public void run() {
                        tapVideoFullscreenButton(tab, videoRect);
                    }
                }, MAX_VIEW_TIME_MS);
        assertNotNull("Fullscreen controls not recreated", fullscreenControls);

        // Wait for it to start (may take some time, hence use a busy wait)
        for (int time = 0; time < MAX_VIEW_TIME_MS; time += VIEW_RETRY_MS) {
            if (isPlayingRemotely()) break;
            sleepNoThrow(VIEW_RETRY_MS);
        }

        checkVideoStarted(TEST_VIDEO_2);
    }

    /**
     * Cast a video but don't pause it. Use for testing reconnection after video finished
     *
     * @throws InterruptedException
     * @throws TimeoutException
     */
    @DisabledTest
    public void testVideoFinishedSetup() throws InterruptedException, TimeoutException {
        castDefaultVideoFromPage(DEFAULT_VIDEO_PAGE);

        // Get the duration of the video
        mVideoDurationMs = getRemoteDurationMs();
    }

    /**
     * Test that once the video has finished we don't automatically reconnect.
     *
     * @throws InterruptedException
     * @throws TimeoutException
     */
    @DisabledTest
    public void testVideoFinished() throws InterruptedException, TimeoutException {
        // Give the video time to finish (actually will probably have already finished,
        // but this makes sure)
        sleepNoThrow(mVideoDurationMs);

        // TODO(aberent) Check that the notification has gone away. Difficult to do until
        // the notification handling is refactored.

        // Put the video into fullscreen and check that the fullscreen controls are not recreated
        Tab tab = getActivity().getActivityTab();

        WebContents webContents = tab.getWebContents();
        waitUntilVideoReady(VIDEO_ELEMENT, webContents);

        Rect videoRect = DOMUtils.getNodeBounds(webContents, VIDEO_ELEMENT);
        Instrumentation instrumentation = getInstrumentation();
        ActivityMonitor monitor = instrumentation.addMonitor(
                ExpandedControllerActivity.class.getCanonicalName(), null, false);

        tapVideoFullscreenButton(tab, videoRect);

        instrumentation.waitForIdleSync();
        Activity fullScreenControls = monitor.getLastActivity();
        if (fullScreenControls == null) {
            fullScreenControls = monitor.waitForActivityWithTimeout(MAX_VIEW_TIME_MS);
        }
        assertNull(fullScreenControls);
    }


    @Override
    protected void setUp() throws Exception {
        mSkipClearAppData = true;
        super.setUp();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityFromLauncher();
    }

}
