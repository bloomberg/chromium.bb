// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.Context;
import android.media.AudioManager;
import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Integration test that checks that autoplay muted doesn't show a notification nor take audio focus
 */
@RetryOnFailure
public class AutoplayMutedNotificationTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String TEST_PATH = "/content/test/data/media/session/autoplay-muted.html";
    private static final String VIDEO_ID = "video";
    private static final int AUDIO_FOCUS_CHANGE_TIMEOUT = 500;  // ms

    private EmbeddedTestServer mTestServer;

    public AutoplayMutedNotificationTest() {
        super(ChromeActivity.class);
    }

    private AudioManager getAudioManager() {
        return (AudioManager) getActivity().getApplicationContext().getSystemService(
                Context.AUDIO_SERVICE);
    }

    private boolean isMediaNotificationVisible() {
        return MediaNotificationManager.hasManagerForTesting(R.id.media_playback_notification);
    }

    private class MockAudioFocusChangeListener implements AudioManager.OnAudioFocusChangeListener {
        private int mAudioFocusState = AudioManager.AUDIOFOCUS_LOSS;

        @Override
        public void onAudioFocusChange(int focusChange) {
            mAudioFocusState = focusChange;
        }

        public int getAudioFocusState() {
            return mAudioFocusState;
        }

        public void requestAudioFocus(int focusType) throws Exception {
            int result = getAudioManager().requestAudioFocus(
                    this, AudioManager.STREAM_MUSIC, focusType);
            if (result != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                fail("Did not get audio focus");
            } else {
                mAudioFocusState = focusType;
            }
        }
    }

    private MockAudioFocusChangeListener mAudioFocusChangeListener;

    @Override
    protected void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        mAudioFocusChangeListener = new MockAudioFocusChangeListener();
        super.setUp();
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityWithURL(mTestServer.getURL(TEST_PATH));
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testBasic() throws Exception {
        Tab tab = getActivity().getActivityTab();

        // Taking audio focus.
        assertEquals(AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        // The page will autoplay the video.
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);

        // Audio focus notification is OS-driven.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);

        // Audio focus was not taken and no notification is visible.
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());
        assertFalse(isMediaNotificationVisible());
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testDoesNotReactToAudioFocus() throws Exception {
        Tab tab = getActivity().getActivityTab();

        // The page will autoplay the video.
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);

        // Taking audio focus.
        assertEquals(AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        // Audio focus notification is OS-driven.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);

        // Video did not pause.
        assertFalse(DOMUtils.isMediaPaused(tab.getWebContents(), VIDEO_ID));

        // Still no notification.
        assertFalse(isMediaNotificationVisible());
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testAutoplayMutedThenUnmute() throws Exception {
        Tab tab = getActivity().getActivityTab();

        // Taking audio focus.
        assertEquals(AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        // The page will autoplay the video.
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);

        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var video = document.querySelector('video');");
        sb.append("  video.muted = false;");
        sb.append("  return video.muted;");
        sb.append("})();");

        // Unmute from script.
        String result = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), sb.toString());
        assertTrue(result.trim().equalsIgnoreCase("false"));

        // Video is paused.
        assertTrue(DOMUtils.isMediaPaused(tab.getWebContents(), VIDEO_ID));

        // Audio focus notification is OS-driven.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);

        // Audio focus was not taken and no notification is visible.
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());
        assertFalse(isMediaNotificationVisible());
    }
}
