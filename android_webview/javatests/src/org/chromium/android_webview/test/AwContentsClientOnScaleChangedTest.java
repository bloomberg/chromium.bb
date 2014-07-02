// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.content.browser.ContentViewCore;

/**
 * Tests for the WebViewClient.onScaleChanged.
 */
public class AwContentsClientOnScaleChangedTest extends AwTestBase {
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
    }

    @SmallTest
    public void testScaleUp() throws Throwable {
        getAwSettingsOnUiThread(mAwContents).setUseWideViewPort(true);
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                CommonResources.ABOUT_HTML, "text/html", false);
        ContentViewCore core = mAwContents.getContentViewCore();
        int callCount = mContentsClient.getOnScaleChangedHelper().getCallCount();
        core.onSizeChanged(
                core.getViewportWidthPix() / 2, core.getViewportHeightPix() / 2,
                core.getViewportWidthPix(), core.getViewportHeightPix());
        // TODO: Investigate on using core.zoomIn();
        mContentsClient.getOnScaleChangedHelper().waitForCallback(callCount);
        assertTrue("Scale ratio:" + mContentsClient.getOnScaleChangedHelper().getLastScaleRatio(),
                mContentsClient.getOnScaleChangedHelper().getLastScaleRatio() < 1);
    }
}
