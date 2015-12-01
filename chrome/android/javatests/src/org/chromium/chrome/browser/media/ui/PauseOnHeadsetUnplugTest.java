// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.content.Intent;
import android.media.AudioManager;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.common.ContentSwitches;

import java.util.concurrent.TimeoutException;

/**
 * Tests for checking whether the media are paused when unplugging the headset
 */
@CommandLineFlags.Add(ContentSwitches.DISABLE_GESTURE_REQUIREMENT_FOR_MEDIA_PLAYBACK)
public class PauseOnHeadsetUnplugTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String TEST_URL =
            "content/test/data/android/media/media-session.html";
    private static final String VIDEO_ID = "long-video";

    public PauseOnHeadsetUnplugTest() {
        super(ChromeActivity.class);
    }

    @SmallTest
    public void testPause()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        Tab tab = getActivity().getActivityTab();

        assertTrue(DOMUtils.isMediaPaused(tab.getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(tab.getWebContents(), VIDEO_ID);
        assertTrue(DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID));
        assertTrue(waitForNotificationReady());

        simulateHeadsetUnplug();
        assertTrue(DOMUtils.waitForMediaPause(tab.getWebContents(), VIDEO_ID));
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityWithURL(TestHttpServerClient.getUrl(TEST_URL));
    }

    private boolean waitForNotificationReady()
            throws InterruptedException {
        return CriteriaHelper.pollForCriteria(new Criteria() {
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
        i.putExtra(MediaButtonReceiver.EXTRA_NOTIFICATION_ID,
                   R.id.media_playback_notification);

        getInstrumentation().getContext().startService(i);
    }
}
