// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContents.VisualStateFlushCallback;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.GraphicsTestUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Visual state related tests.
 */
public class VisualStateTest extends AwTestBase {

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testFlushVisualStateCallbackIsReceived() throws Throwable {
        AwTestContainerView testContainer = createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testContainer.getAwContents();
        loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), CommonResources.ABOUT_HTML);
        final CallbackHelper ch = new CallbackHelper();
        final int chCount = ch.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                awContents.flushVisualState(new AwContents.VisualStateFlushCallback() {
                    @Override
                    public void onComplete() {
                        ch.notifyCalled();
                    }

                    @Override
                    public void onFailure() {
                        fail("onFailure received");
                    }
                });
            }
        });
        ch.waitForCallback(chCount);
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testFlushVisualStateCallbackWaitsForContentsToBeOnScreen() throws Throwable {
        // This test loads a page with a blue background color. It then waits for the DOM tree
        // in blink to contain the contents of the blue page (which happens when the onPageFinished
        // event is received). It then flushes the contents and verifies that the blue page
        // background color is drawn when the flush callback is received.
        final LoadUrlParams bluePageUrl = createTestPageUrl("blue");
        final CountDownLatch testFinishedSignal = new CountDownLatch(1);

        final AtomicReference<AwContents> awContentsRef = new AtomicReference<>();
        final AwTestContainerView testView =
                createAwTestContainerViewOnMainSync(new TestAwContentsClient() {
                    @Override
                    public void onPageFinished(String url) {
                        if (bluePageUrl.getUrl().equals(url)) {
                            awContentsRef.get().flushVisualState(new VisualStateFlushCallback() {
                                @Override
                                public void onFailure() {
                                    fail("onFailure received");
                                }

                                @Override
                                public void onComplete() {
                                    Bitmap blueScreenshot = GraphicsTestUtils.drawAwContents(
                                            awContentsRef.get(), 1, 1);
                                    assertEquals(Color.BLUE, blueScreenshot.getPixel(0, 0));
                                    testFinishedSignal.countDown();
                                }
                            });
                        }
                    }
                });
        final AwContents awContents = testView.getAwContents();
        awContentsRef.set(awContents);

        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                awContents.setBackgroundColor(Color.RED);
                awContents.loadUrl(bluePageUrl);

                // We have just loaded the blue page, but the graphics pipeline is asynchronous
                // so at this point the WebView still draws red, ie. the View background color.
                // Only when the flush callback is received will we know for certain that the
                // blue page contents are on screen.
                Bitmap redScreenshot = GraphicsTestUtils.drawAwContents(
                        awContentsRef.get(), 1, 1);
                assertEquals(Color.RED, redScreenshot.getPixel(0, 0));
            }
        });

        assertTrue(testFinishedSignal.await(AwTestBase.WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private static final LoadUrlParams createTestPageUrl(String backgroundColor) {
        return LoadUrlParams.createLoadDataParams(
                "<html><body bgcolor=" + backgroundColor + "></body></html>", "text/html", false);
    }
}
