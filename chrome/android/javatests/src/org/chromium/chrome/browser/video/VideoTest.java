// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video;

import android.os.Environment;
import android.test.suitebuilder.annotation.LargeTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 *  Simple tests of html5 video.
 */
public class VideoTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    public VideoTest() {
        super(ChromeActivity.class);
        mSkipCheckHttpServer = true;
    }

    @Feature({"Media", "Media-Video", "Main"})
    @LargeTest
    public void testLoadMediaUrl() throws InterruptedException, TimeoutException {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
        try {
            Tab tab = getActivity().getActivityTab();
            TabTitleObserver titleObserver = new TabTitleObserver(tab, "ready_to_play");
            loadUrl(testServer.getURL("/chrome/test/data/android/media/video-play.html"));
            titleObserver.waitForTitleUpdate(5);
            assertEquals("ready_to_play", tab.getTitle());

            titleObserver = new TabTitleObserver(tab, "ended");
            DOMUtils.clickNode(this, tab.getContentViewCore(), "button1");
            // Now the video will play for 5 secs.
            // Makes sure that the video ends and title was changed.
            titleObserver.waitForTitleUpdate(15);
            assertEquals("ended", tab.getTitle());
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
