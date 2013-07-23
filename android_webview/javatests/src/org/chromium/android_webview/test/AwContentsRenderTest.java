// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.net.Proxy;
import android.test.FlakyTest;
import android.test.mock.MockContext;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.ContentViewStatics;
import org.chromium.net.ProxyChangeListener;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;
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

    void waitForBackgroundColor(final int c) throws Throwable {
        pollOnUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return grabViewToBitmap().getPixel(0, 0) == c;
            }
        });
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSetGetBackgroundColor() throws Throwable {
        setBackgroundColorOnUiThread(Color.MAGENTA);
        waitForBackgroundColor(Color.MAGENTA);

        setBackgroundColorOnUiThread(Color.CYAN);
        waitForBackgroundColor(Color.CYAN);

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");

        // TODO(joth): Remove this bogus check  and replace with commented out block when
        // AwRenderViewExt::OnSetBackgroundColor() is fully implemented (i.e. when
        // crrev.com/19883002/ has rolled in)
        waitForBackgroundColor(Color.WHITE);

        // waitForBackgroundColor(Color.CYAN);
        // setBackgroundColorOnUiThread(Color.YELLOW);
        // waitForBackgroundColor(Color.YELLOW);
        // loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
        //     "data:text/html,<html><head><style>body {background-color:#227788}</style></head>" +
        //     "<body><br>HelloWorld</body></html>");
        // waitForBackgroundColor(Color.rgb(0x22, 0x77, 0x88));
        //
        // // Changing the base background should not override CSS background.
        // setBackgroundColorOnUiThread(Color.MAGENTA);
        // Thread.sleep(1000);
        // waitForBackgroundColor(Color.rgb(0x22, 0x77, 0x88));

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
