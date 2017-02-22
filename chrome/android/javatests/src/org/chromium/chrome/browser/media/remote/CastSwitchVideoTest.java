// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import android.graphics.Rect;
import android.support.test.filters.LargeTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.TimeoutException;

/**
 * Test that other videos are played locally when casting
 */
public class CastSwitchVideoTest extends CastTestBase {

    private static final String VIDEO_ELEMENT_2 = "video2";

    @Feature({"VideoFling"})
    @LargeTest
    @RetryOnFailure  // crbug.com/623526
    public void testPlayNewVideoInNewTab() throws InterruptedException, TimeoutException {
        checkPlaySecondVideo(DEFAULT_VIDEO_PAGE, VIDEO_ELEMENT, new Runnable() {
            @Override
            public void run() {
                try {
                    loadUrlInNewTab(getTestServer().getURL(TEST_VIDEO_PAGE_2));
                    playVideoFromCurrentTab(VIDEO_ELEMENT);
                } catch (Exception e) {
                    fail("Failed to start second video; " + e.getMessage());
                }
            }
        });
    }

    @Feature({"VideoFling"})
    @LargeTest
    @RetryOnFailure  // crbug.com/623526
    public void testPlayNewVideoNewPageSameTab() throws InterruptedException, TimeoutException {
        checkPlaySecondVideo(DEFAULT_VIDEO_PAGE, VIDEO_ELEMENT, new Runnable() {
            @Override
            public void run() {
                try {
                    loadUrl(getTestServer().getURL(TEST_VIDEO_PAGE_2));
                    playVideoFromCurrentTab(VIDEO_ELEMENT);
                } catch (Exception e) {
                    fail("Failed to start second video; " + e.getMessage());
                }
            }
        });
    }

    @Feature({"VideoFling"})
    @LargeTest
    @RetryOnFailure  // crbug.com/623526
    public void testPlayTwoVideosSamePage() throws InterruptedException, TimeoutException {
        checkPlaySecondVideo(TWO_VIDEO_PAGE, VIDEO_ELEMENT_2, new Runnable() {
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

    @Feature({"VideoFling"})
    @LargeTest
    @RetryOnFailure  // crbug.com/623526
    public void testCastNewVideoInNewTab() throws InterruptedException, TimeoutException {
        checkCastSecondVideo(DEFAULT_VIDEO_PAGE, new Runnable() {
            @Override
            public void run() {
                try {
                    loadUrlInNewTab(getTestServer().getURL(TEST_VIDEO_PAGE_2));
                    castVideoFromCurrentTab(VIDEO_ELEMENT);
                } catch (Exception e) {
                    fail("Failed to start second video; " + e.getMessage());
                }
            }
        });
    }

    @Feature({"VideoFling"})
    @LargeTest
    @RetryOnFailure  // crbug.com/623526
    public void testCastNewVideoNewPageSameTab() throws InterruptedException, TimeoutException {
        checkCastSecondVideo(DEFAULT_VIDEO_PAGE, new Runnable() {
            @Override
            public void run() {
                try {
                    loadUrl(getTestServer().getURL(TEST_VIDEO_PAGE_2));
                    castVideoFromCurrentTab(VIDEO_ELEMENT);
                } catch (Exception e) {
                    fail("Failed to start second video; " + e.getMessage());
                }
            }
        });
    }

    @Feature({"VideoFling"})
    @LargeTest
    @RetryOnFailure  // crbug.com/623526
    public void testCastTwoVideosSamePage() throws InterruptedException, TimeoutException {
        checkCastSecondVideo(TWO_VIDEO_PAGE, new Runnable() {
            @Override
            public void run() {
                try {
                    castVideoFromCurrentTab(VIDEO_ELEMENT_2);
                } catch (Exception e) {
                    fail("Failed to start second video; " + e.getMessage());
                }
            }
        });
    }

    private void checkPlaySecondVideo(
            String firstVideoPage, String secondVideoId, final Runnable startSecondVideo)
                    throws InterruptedException, TimeoutException {
        // TODO(aberent) Checking position is flaky, because it is timing dependent, but probably
        // a good idea in principle. Need to find a way of unflaking it.
        // int position = castAndPauseDefaultVideoFromPage(firstVideoPage);
        castAndPauseDefaultVideoFromPage(firstVideoPage);

        startSecondVideo.run();

        // Check that we are still casting the default video
        assertEquals("The first video is not casting", getTestServer().getURL(DEFAULT_VIDEO),
                getUriPlaying());

        // Check that the second video is still there and paused
        final Tab tab = getActivity().getActivityTab();
        WebContents webContents = tab.getWebContents();
        assertFalse("Other video is not playing",
                DOMUtils.isMediaPaused(webContents, secondVideoId));
    }

    private void checkCastSecondVideo(String firstVideoPage,  final Runnable startSecondVideo)
            throws InterruptedException, TimeoutException {
        // TODO(aberent) Checking position is flaky, because it is timing dependent, but probably
        // a good idea in principle. Need to find a way of unflaking it.
        // int position = castAndPauseDefaultVideoFromPage(firstVideoPage);
        castAndPauseDefaultVideoFromPage(firstVideoPage);

        startSecondVideo.run();

        // Check that we switch to playing the right video
        checkVideoStarted(TEST_VIDEO_2);
    }

    private void castVideoFromCurrentTab(String videoElement) throws InterruptedException,
            TimeoutException {
        final Tab tab = getActivity().getActivityTab();
        WebContents webContents = tab.getWebContents();
        waitUntilVideoReady(videoElement, webContents);
        Rect videoRect = DOMUtils.getNodeBounds(webContents, videoElement);

        castVideoAndWaitUntilPlaying(CAST_TEST_ROUTE, tab, videoRect);
    }

    private void playVideoFromCurrentTab(String videoElement) throws InterruptedException,
            TimeoutException {
        final Tab tab = getActivity().getActivityTab();
        WebContents webContents = tab.getWebContents();

        waitUntilVideoReady(videoElement, webContents);

        // Need to click on the video first to overcome the user gesture requirement.
        DOMUtils.clickNode(tab.getContentViewCore(), videoElement);
        DOMUtils.playMedia(webContents, videoElement);
        DOMUtils.waitForMediaPlay(webContents, videoElement);
    }

}
