// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.content.Intent;
import android.media.AudioManager;
import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 * Tests for checking whether the media are paused when unplugging the headset
 */
@CommandLineFlags.Add(ContentSwitches.DISABLE_GESTURE_REQUIREMENT_FOR_MEDIA_PLAYBACK)
public class PauseOnHeadsetUnplugTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String TEST_PATH =
            "/content/test/data/media/session/media-session.html";
    private static final String VIDEO_ID = "long-video";

    private EmbeddedTestServer mTestServer;

    public PauseOnHeadsetUnplugTest() {
        super(ChromeActivity.class);
    }

    @SmallTest
    @RetryOnFailure
    public void testPause()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        Tab tab = getActivity().getActivityTab();

        assertTrue(DOMUtils.isMediaPaused(tab.getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(tab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);
        waitForNotificationReady();

        simulateHeadsetUnplug();
        DOMUtils.waitForMediaPauseBeforeEnd(tab.getWebContents(), VIDEO_ID);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityWithURL(mTestServer.getURL(TEST_PATH));
    }

    @Override
    protected void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        super.setUp();
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    private void waitForNotificationReady() {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return MediaNotificationManager.hasManagerForTesting(
                        R.id.media_playback_notification);
            }
        });
    }

    private void simulateHeadsetUnplug() {
        Intent i = new Intent(getInstrumentation().getTargetContext(),
                              MediaNotificationManager.PlaybackListenerService.class);
        i.setAction(AudioManager.ACTION_AUDIO_BECOMING_NOISY);

        getInstrumentation().getContext().startService(i);
    }
}
