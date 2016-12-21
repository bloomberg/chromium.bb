// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.ThreadUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

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

    @SmallTest
    public void testScaleUp() throws Throwable {
        getAwSettingsOnUiThread(mAwContents).setSupportZoom(true);
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageFrom(
                        "<meta name=\"viewport\" content=\"initial-scale=1.0, "
                                + " minimum-scale=0.5, maximum-scale=2, user-scalable=yes\" />",
                        "testScaleUp test page body"),
                "text/html", false);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mAwContents.canZoomIn();
            }
        });
        int callCount = mContentsClient.getOnScaleChangedHelper().getCallCount();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                assertTrue(mAwContents.zoomIn());
            }
        });
        mContentsClient.getOnScaleChangedHelper().waitForCallback(callCount);
        assertTrue("Scale ratio:" + mContentsClient.getOnScaleChangedHelper().getLastScaleRatio(),
                mContentsClient.getOnScaleChangedHelper().getLastScaleRatio() > 1);
    }
}
