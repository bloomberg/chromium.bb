// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 * Simple HTML5 audio tests.
 */
public class AudioTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    private EmbeddedTestServer mTestServer;

    public AudioTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /**
     * Test playing a small mp3 audio file.
     * @Feature({"Media", "Media-Audio", "Main"})
     * @MediumTest
     */
    // TODO(jbudorick): Attempt to reenable this after the server switch has stabilized.
    @FlakyTest(message = "crbug.com/331122")
    public void testPlayMp3() throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActivityTab();
        TabTitleObserver titleObserver = new TabTitleObserver(tab, "ready_to_play");
        loadUrl(mTestServer.getURL("/chrome/test/data/android/media/audio-play.html"));
        titleObserver.waitForTitleUpdate(5);
        assertEquals("ready_to_play", tab.getTitle());

        titleObserver = new TabTitleObserver(tab, "ended");
        DOMUtils.clickNode(tab.getContentViewCore(), "button1");

        // Make sure that the audio playback "ended" and title is changed.
        titleObserver.waitForTitleUpdate(15);
        assertEquals("ended", tab.getTitle());
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
