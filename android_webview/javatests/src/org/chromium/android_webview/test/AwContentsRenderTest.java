// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;

import java.util.concurrent.Callable;

/**
 * AwContents rendering / pixel tests.
 */
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

    Bitmap grabViewToBitmap() {
        final Bitmap result = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        mAwContents.onDraw(new android.graphics.Canvas(result));
        return result;
    }

    int sampleBackgroundColorOnUiThread() throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() throws Exception {
                return grabViewToBitmap().getPixel(0, 0);
            }
        });
    }

    void pollForBackgroundColor(final int c) throws Throwable {
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return sampleBackgroundColorOnUiThread() == c;
            }
        });
    }

    /*
    @SmallTest
    @Feature({"AndroidWebView"})
    crbug.com/384559
    */
    @DisabledTest
    public void testSetGetBackgroundColor() throws Throwable {
        setBackgroundColorOnUiThread(Color.MAGENTA);
        pollForBackgroundColor(Color.MAGENTA);

        setBackgroundColorOnUiThread(Color.CYAN);
        pollForBackgroundColor(Color.CYAN);

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");
        assertEquals(Color.CYAN, sampleBackgroundColorOnUiThread());

        setBackgroundColorOnUiThread(Color.YELLOW);
        pollForBackgroundColor(Color.YELLOW);

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
            "data:text/html,<html><head><style>body {background-color:#227788}</style></head>" +
            "<body><br>HelloWorld</body></html>");
        pollForBackgroundColor(Color.rgb(0x22, 0x77, 0x88));

        // Changing the base background should not override CSS background.
        setBackgroundColorOnUiThread(Color.MAGENTA);
        assertEquals(Color.rgb(0x22, 0x77, 0x88), sampleBackgroundColorOnUiThread());
        // ...setting the background is asynchronous, so pause a bit and retest just to be sure.
        Thread.sleep(500);
        assertEquals(Color.rgb(0x22, 0x77, 0x88), sampleBackgroundColorOnUiThread());
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
