// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.android_webview.test.util.VideoSurfaceViewUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.Callable;

/**
 * Tests pages with multiple videos.
 */
public class MultipleVideosTest extends AwTestBase {
    private static final String MULTIPLE_VIDEOS_TEST_URL =
            "file:///android_asset/multiple_videos_test.html";
    // These values must be kept in sync with the strings in above html.
    private static final String FIRST_VIDEO_ID = "firstVideo";
    private static final String SECOND_VIDEO_ID = "secondVideo";
    private static final String PLAY_FIRST_BUTTON_ID = "playFirstButton";
    private static final String PLAY_SECOND_BUTTON_ID = "playSecondButton";

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private ContentViewCore mContentViewCore;

    protected void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = createAwTestContainerViewOnMainSync(mContentsClient);
        mContentViewCore = mTestContainerView.getContentViewCore();
        enableJavaScriptOnUiThread(mTestContainerView.getAwContents());
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testFirstVideoPausesWhenSecondVideoStarts()
            throws Throwable {
        // To test video hole surfaces we must force video overlay before loading page.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mTestContainerView.getAwContents().getSettings().setForceVideoOverlayForTests(true);
            }
        });

        loadTestPage();

        // Play the first video.
        tapFirstPlayButton();
        assertTrue(DOMUtils.waitForMediaPlay(getWebContentsOnUiThread(), FIRST_VIDEO_ID));

        // Verify that there is one video hole surface.
        VideoSurfaceViewUtils.pollAndAssertContainsOneVideoHoleSurfaceView(this,
                mTestContainerView);

        // Start the second video.
        tapSecondPlayButton();
        assertTrue(DOMUtils.waitForMediaPlay(getWebContentsOnUiThread(), SECOND_VIDEO_ID));

        // Verify that the first video pauses once the second video starts.
        assertFalse(DOMUtils.isMediaPaused(getWebContentsOnUiThread(), SECOND_VIDEO_ID));
        assertTrue(DOMUtils.isMediaPaused(getWebContentsOnUiThread(), FIRST_VIDEO_ID));

        // Verify that there is still only one video hole surface.
        VideoSurfaceViewUtils.pollAndAssertContainsOneVideoHoleSurfaceView(this,
                mTestContainerView);
    }

    private void loadTestPage() throws Exception {
        loadUrlSync(mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), MULTIPLE_VIDEOS_TEST_URL);
    }

    private void tapFirstPlayButton() throws Exception {
        DOMUtils.clickNode(this, mContentViewCore, PLAY_FIRST_BUTTON_ID);
    }

    private void tapSecondPlayButton() throws Exception {
        DOMUtils.clickNode(this, mContentViewCore, PLAY_SECOND_BUTTON_ID);
    }

    private WebContents getWebContentsOnUiThread() {
        try {
            return runTestOnUiThreadAndGetResult(new Callable<WebContents>() {
                @Override
                public WebContents call() throws Exception {
                    return mContentViewCore.getWebContents();
                }
            });
        } catch (Exception e) {
            fail(e.getMessage());
            return null;
        }
    }
}
