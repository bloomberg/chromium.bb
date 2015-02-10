// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Color;
import android.os.Build;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.GraphicsTestUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;


/**
 * AwContents rendering / pixel tests.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT)
public class AwContentsRenderTest extends AwTestBase {

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    void setBackgroundColorOnUiThread(final int c) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAwContents.setBackgroundColor(c);
            }
        });
    }

    /*
    @SmallTest
    @Feature({"AndroidWebView"})
    */
    public void testSetGetBackgroundColor() throws Throwable {
        setBackgroundColorOnUiThread(Color.MAGENTA);
        GraphicsTestUtils.pollForBackgroundColor(mAwContents, Color.MAGENTA);

        setBackgroundColorOnUiThread(Color.CYAN);
        GraphicsTestUtils.pollForBackgroundColor(mAwContents, Color.CYAN);

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");
        assertEquals(Color.CYAN, GraphicsTestUtils.sampleBackgroundColorOnUiThread(mAwContents));

        setBackgroundColorOnUiThread(Color.YELLOW);
        GraphicsTestUtils.pollForBackgroundColor(mAwContents, Color.YELLOW);

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                "data:text/html,<html><head><style>body {background-color:#227788}</style></head>"
                + "<body></body></html>");
        GraphicsTestUtils.pollForBackgroundColor(mAwContents, Color.rgb(0x22, 0x77, 0x88));

        // Changing the base background should not override CSS background.
        setBackgroundColorOnUiThread(Color.MAGENTA);
        assertEquals(Color.rgb(0x22, 0x77, 0x88),
                GraphicsTestUtils.sampleBackgroundColorOnUiThread(mAwContents));
        // ...setting the background is asynchronous, so pause a bit and retest just to be sure.
        Thread.sleep(500);
        assertEquals(Color.rgb(0x22, 0x77, 0x88),
                GraphicsTestUtils.sampleBackgroundColorOnUiThread(mAwContents));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPictureListener() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAwContents.enableOnNewPicture(true, true);
            }
        });

        int pictureCount = mContentsClient.getPictureListenerHelper().getCallCount();
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");
        mContentsClient.getPictureListenerHelper().waitForCallback(pictureCount, 1);
        // Invalidation only, so picture should be null.
        assertNull(mContentsClient.getPictureListenerHelper().getPicture());
    }
}
