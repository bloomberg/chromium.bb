// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.graphics.Rect;
import android.test.suitebuilder.annotation.LargeTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.concurrent.TimeUnit;

public class TestsJavaScriptEvalTest extends ContentShellTestBase {
    private static final int WAIT_TIMEOUT_SECONDS = 2;
    private static final String JSTEST_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><script>" +
            "  function foobar() { return 'foobar'; }" +
            "</script></head>" +
            "<body><button id=\"test\">Test button</button></body></html>");

    public TestsJavaScriptEvalTest() {
    }

    /**
     * Tests that evaluation of JavaScript for test purposes (using JavaScriptUtils, DOMUtils etc)
     * works even in presence of "background" (non-test-initiated) JavaScript evaluation activity.
     */
    @LargeTest
    @Feature({"Browser"})
    public void testJavaScriptEvalIsCorrectlyOrdered()
            throws InterruptedException, Exception, Throwable {
        launchContentShellWithUrl(JSTEST_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());

        final ContentView view = getActivity().getActiveContentView();
        final TestCallbackHelperContainer viewClient =
                new TestCallbackHelperContainer(view);

        for(int i = 0; i < 30; ++i) {
            for(int j = 0; j < 10; ++j) {
                // Start evaluation of a JavaScript script -- we don't need a result.
                view.evaluateJavaScript("foobar();");
            }
            // DOMUtils does need to evaluate a JavaScript and get its result to get DOM bounds.
            assertNotNull("Failed to get bounds",
                    DOMUtils.getNodeBounds(view, viewClient, "test"));
        }
    }
}
