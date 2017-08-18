// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;

/**
 * Test that a page with a non-Chrome media codec can playback correctly; this
 * test is *NOT* exhaustive, but merely spot checks a single instance.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class PlatformMediaCodecTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private ContentViewCore mContentViewCore;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mContentViewCore = mTestContainerView.getContentViewCore();
        mActivityTestRule.enableJavaScriptOnUiThread(mTestContainerView.getAwContents());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @DisabledTest(message = "crbug.com/620890")
    public void testCanPlayPlatformMediaCodecs() throws Throwable {
        mActivityTestRule.loadUrlSync(mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(),
                "file:///android_asset/platform-media-codec-test.html");
        DOMUtils.clickNode(mContentViewCore, "playButton");
        DOMUtils.waitForMediaPlay(getWebContentsOnUiThread(), "videoTag");
    }

    private WebContents getWebContentsOnUiThread() {
        try {
            return mActivityTestRule.runTestOnUiThreadAndGetResult(
                    () -> mContentViewCore.getWebContents());
        } catch (Exception e) {
            Assert.fail(e.getMessage());
            return null;
        }
    }
}
