// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.SystemClock;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.AssertionFailedError;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentReadbackHandler.GetBitmapCallback;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Tests that we can readback the contents of a ContentView.
 */
public class ContentViewReadbackTest extends ContentShellTestBase {

    private static final int COLOR_THRESHOLD = 8;
    private static final long WAIT_FOR_READBACK_TIMEOUT =  scaleTimeout(10000);

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(
                "<html style=\"background: #00f;\"><head><style>body { height: 5000px; }</style>" +
                "</head></html>"));
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());
    }

    private static void assertEqualColor(int actual, int expected, int threshold) {
        int deltaR = Math.abs(Color.red(actual) - Color.red(expected));
        int deltaG = Math.abs(Color.green(actual) - Color.green(expected));
        int deltaB = Math.abs(Color.blue(actual) - Color.blue(expected));
        if (deltaR > threshold || deltaG > threshold || deltaB > threshold) {
            throw new AssertionFailedError(
                    "Color does not match; expected " + expected + ", got " + actual);
        }
    }

    private void assertWaitForYScroll(final int previousYScroll) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getContentViewCore().getNativeScrollYForTest() > previousYScroll;
            }
        }));
    }

    private void assertWaitForReadback(final AtomicBoolean readbackDone)
            throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return readbackDone.get();
            }
        }, WAIT_FOR_READBACK_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL));
    }

    /**
     * Test to make sure the screenshots captured of the ContentView are not blank.
     *
     * @throws InterruptedException
     */
    @SmallTest
    @Feature({"Main"})
    public void testScreenshotIsNotBlank() throws Throwable {
        final AtomicBoolean readbackDone = new AtomicBoolean(false);
        final AtomicInteger color = new AtomicInteger();

        // We wait on the fling to make sure that the compositor side has a layer available by the
        // time we reach readback.
        int previousYScroll = getContentViewCore().getNativeScrollYForTest();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getContentViewCore().flingForTest(SystemClock.uptimeMillis(), 0, 0, 0, -100);
            }
        });
        assertWaitForYScroll(previousYScroll);

        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                ContentReadbackHandler contentReadbackHandler = new ContentReadbackHandler() {
                    @Override
                    protected boolean readyForReadback() {
                        return true;
                    }
                };

                GetBitmapCallback callback = new GetBitmapCallback() {
                    @Override
                    public void onFinishGetBitmap(Bitmap bitmap) {
                        assertNotNull("Readback did not return valid bitmap", bitmap);
                        // Verify a pixel in the center of the screenshot.
                        color.set(bitmap.getPixel(bitmap.getWidth() / 2, bitmap.getHeight() / 2));
                        readbackDone.set(true);
                    }
                };

                contentReadbackHandler.initNativeContentReadbackHandler();
                contentReadbackHandler.getContentBitmapAsync(1.0f, new Rect(), getContentViewCore(),
                        callback);
            }
        });
        assertWaitForReadback(readbackDone);

        assertEqualColor(color.get(), Color.BLUE, COLOR_THRESHOLD);
    }
}
