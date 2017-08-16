// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.Callable;

/**
 * Test that a page with a non-Chrome media codec can playback correctly; this
 * test is *NOT* exhaustive, but merely spot checks a single instance.
 */
public class PlatformMediaCodecTest extends AwTestBase {
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
    @DisabledTest(message = "crbug.com/620890")
    public void testCanPlayPlatformMediaCodecs() throws Throwable {
        loadUrlSync(mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper(),
                "file:///android_asset/platform-media-codec-test.html");
        DOMUtils.clickNode(mContentViewCore, "playButton");
        DOMUtils.waitForMediaPlay(getWebContentsOnUiThread(), "videoTag");
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
