// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import android.test.FlakyTest;
import android.test.suitebuilder.annotation.LargeTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.TimeoutException;

/**
 * Test switching videos when casting
 */
public class CastSwitchVideoTest extends CastTestBase {

    private static final String VIDEO_ELEMENT_2 = "video2";

    @Feature({"VideoFling"})
    // Appears to be flaky, see crbug.com/515085
    // @LargeTest
    @FlakyTest
    public void testNewVideoInNewTab() throws InterruptedException, TimeoutException {
        // This won't currently work in document mode because we can't create new tabs
        if (FeatureUtilities.isDocumentMode(getActivity())) return;
        checkSwitchVideo(DEFAULT_VIDEO_PAGE, new Runnable() {
            @Override
            public void run() {
                try {
                    loadUrlInNewTab(TestHttpServerClient.getUrl(TEST_VIDEO_PAGE_2));
                    playVideoFromCurrentTab(VIDEO_ELEMENT);
                } catch (Exception e) {
                    fail("Failed to start second video; " + e.getMessage());
                }
            }
        });
    }

    @Feature({"VideoFling"})
    @LargeTest
    public void testNewVideoNewPageSameTab() throws InterruptedException, TimeoutException {
        checkSwitchVideo(DEFAULT_VIDEO_PAGE, new Runnable() {
            @Override
            public void run() {
                try {
                    loadUrl(TestHttpServerClient.getUrl(TEST_VIDEO_PAGE_2));
                    playVideoFromCurrentTab(VIDEO_ELEMENT);
                } catch (Exception e) {
                    fail("Failed to start second video; " + e.getMessage());
                }
            }
        });
    }

    @Feature({"VideoFling"})
    @LargeTest
    public void testTwoVideosSamePage() throws InterruptedException, TimeoutException {
        checkSwitchVideo(TWO_VIDEO_PAGE, new Runnable() {
            @Override
            public void run() {
                try {
                    playVideoFromCurrentTab(VIDEO_ELEMENT_2);
                } catch (Exception e) {
                    fail("Failed to start second video; " + e.getMessage());
                }
            }
        });
    }

    private void checkSwitchVideo(String firstVideoPage, final Runnable startSecondVideo)
            throws InterruptedException, TimeoutException {
        // TODO(aberent) Checking position is flaky, because it is timing dependent, but probably
        // a good idea in principle. Need to find a way of unflaking it.
        // int position = castAndPauseDefaultVideoFromPage(firstVideoPage);
        castAndPauseDefaultVideoFromPage(firstVideoPage);

        startSecondVideo.run();

        // Check that we switch to playing the right video
        checkVideoStarted(TEST_VIDEO_2);

        // Check we are back at the start of the video
        RemoteMediaPlayerController controller = RemoteMediaPlayerController.getIfExists();
        assertNotNull("No controller", controller);

        // TODO(aberent) Check position.
        // assertTrue("Position in video wrong", getPosition() < position);
    }

    private void playVideoFromCurrentTab(String videoElement) throws InterruptedException,
            TimeoutException {
        // Start playing the video by tapping at its centre
        final Tab tab = getActivity().getActivityTab();
        WebContents webContents = tab.getWebContents();

        waitUntilVideoReady(videoElement, webContents);

        DOMUtils.clickNode(this, tab.getContentViewCore(), videoElement);
    }

}
