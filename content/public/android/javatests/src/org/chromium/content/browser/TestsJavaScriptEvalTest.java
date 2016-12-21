// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.support.test.filters.LargeTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_shell_apk.ContentShellTestBase;

/**
 * Integration tests for JavaScript execution.
 */
public class TestsJavaScriptEvalTest extends ContentShellTestBase {
    private static final String JSTEST_URL = UrlUtils.encodeHtmlDataUri("<html><head><script>"
            + "  function foobar() { return 'foobar'; }"
            + "</script></head>"
            + "<body><button id=\"test\">Test button</button></body></html>");

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
        waitForActiveShellToBeDoneLoading();

        final WebContents webContents = getWebContents();
        for (int i = 0; i < 30; ++i) {
            for (int j = 0; j < 10; ++j) {
                // Start evaluation of a JavaScript script -- we don't need a result.
                webContents.evaluateJavaScriptForTests("foobar();", null);
            }
            // DOMUtils does need to evaluate a JavaScript and get its result to get DOM bounds.
            assertNotNull("Failed to get bounds",
                    DOMUtils.getNodeBounds(webContents, "test"));
        }
    }
}
