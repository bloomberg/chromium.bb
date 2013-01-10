// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.android_webview.test.util.CommonResources;

import android.util.Log;

/**
 * Tests for the WebViewClient.onScaleChanged.
 */
public class AwContentsClientOnScaleChangedTest extends AndroidWebViewTestBase {
    private static class OnScaleChangedHelper extends CallbackHelper {
       private float mLastScaleRatio;

       public void notifyCalled(float ratio) {
           super.notifyCalled();
           mLastScaleRatio = ratio;
       }
    }

    private static class TestAwContentsClient
            extends org.chromium.android_webview.test.TestAwContentsClient {
         private OnScaleChangedHelper mOnScaleChangedHelper = new OnScaleChangedHelper();
         @Override
         public void onScaleChanged(float oldScale, float newScale) {
             // We should not be getting any messages from scrolling.
             assertTrue(oldScale != newScale);
             mOnScaleChangedHelper.notifyCalled(newScale / oldScale);
         }
    }

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
        getContentSettingsOnUiThread(mAwContents).setUseWideViewPort(true);
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
             CommonResources.ABOUT_HTML, "text/html", false);
        ContentViewCore core = mAwContents.getContentViewCore();
        int callCount = mContentsClient.mOnScaleChangedHelper.getCallCount();
        core.onSizeChanged(core.getWidth() / 2, core.getHeight() / 2,
             core.getWidth(), core.getHeight());
        // TODO: Investigate on using core.zoomIn();
        mContentsClient.mOnScaleChangedHelper.waitForCallback(callCount);
        assertTrue(mContentsClient.mOnScaleChangedHelper.mLastScaleRatio < 1);
    }
}
