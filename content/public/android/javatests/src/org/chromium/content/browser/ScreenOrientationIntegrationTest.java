// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.concurrent.TimeoutException;

/**
 * Integration tests for Screen Orientation API (and legacy API).
 */
public class ScreenOrientationIntegrationTest extends ContentShellTestBase {

    private static final String DEFAULT_URL = UrlUtils.encodeHtmlDataUri(
            "<html><script>var changes=0;</script>" +
            "<body onorientationchange='changes++;'>foo</body>" +
            "</html>");

    private ContentView mContentView;

    /**
     * Returns the screen orientation as seen by |window.orientation|.
     */
    private int getWindowOrientation()
            throws InterruptedException, TimeoutException {
        return Integer.parseInt(
            JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    mContentView.getContentViewCore(),
                    "window.orientation"));
    }

    /**
     * Returns the number of times the web content received a orientationchange
     * event.
     */
    private int getWindowOrientationChangeCount()
            throws InterruptedException, TimeoutException {
        return Integer.parseInt(
            JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    mContentView.getContentViewCore(),
                    "changes"));
    }

    /**
     * Simulate a screen orientation change for the web content.
     */
    private void updateScreenOrientationForContent(int orientation) {
        mContentView.getContentViewCore().sendOrientationChangeEvent(orientation);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        ContentShellActivity activity = launchContentShellWithUrl(DEFAULT_URL);
        waitForActiveShellToBeDoneLoading();

        mContentView = activity.getActiveContentView();
    }

    @SmallTest
    @Feature({"ScreenOrientation"})
    public void testNoOp() throws Throwable {
        assertEquals(0, getWindowOrientationChangeCount());
    }

    @SmallTest
    @Feature({"ScreenOrientation"})
    public void testExpectedValues() throws Throwable {
        int[] values = { 90, -90, 180, 0, 90 };

        // The first value should depend on the current orientation.
        values[0] = getWindowOrientation() == 0 ? 90 : 0;

        for (int i = 0; i < values.length; ++i) {
            updateScreenOrientationForContent(values[i]);
            assertEquals(values[i], getWindowOrientation());
            assertEquals(i + 1, getWindowOrientationChangeCount());
        }
    }

    // We can't test unexpected value because it is branching to a NOTREACHED().

    @SmallTest
    @Feature({"ScreenOrientation"})
    public void testNoChange() throws Throwable {
        // The target angle for that test should depend on the current orientation.
        int angle = getWindowOrientation() == 0 ? 90 : 0;

        updateScreenOrientationForContent(angle);
        assertEquals(angle, getWindowOrientation());
        assertEquals(1, getWindowOrientationChangeCount());

        updateScreenOrientationForContent(angle);
        assertEquals(angle, getWindowOrientation());
        assertEquals(1, getWindowOrientationChangeCount());

        updateScreenOrientationForContent(angle);
        assertEquals(angle, getWindowOrientation());
        assertEquals(1, getWindowOrientationChangeCount());
    }
}
