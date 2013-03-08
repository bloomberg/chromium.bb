// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_shell.ContentShellTestBase;

/*
 * Tests that we can scroll and fling a ContentView running inside ContentShell.
 */
public class ContentViewScrollingTest extends ContentShellTestBase {

    private static final String LARGE_PAGE = UrlUtils.encodeHtmlDataUri(
            "<html><head>" +
            "<meta name=\"viewport\" content=\"width=device-width, " +
            "initial-scale=1.0, maximum-scale=1.0\" />" +
            "<style>body { width: 5000px; height: 5000px; }</style></head>" +
            "<body>Lorem ipsum dolor sit amet, consectetur adipiscing elit.</body>" +
            "</html>");

    private void assertWaitForPageScaleFactor(final float scale) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getContentViewCore().getScale() == scale;
            }
        }));
    }

    @SmallTest
    @Feature({"Android-WebView"})
    public void testFling() throws Throwable {
        launchContentShellWithUrl(LARGE_PAGE);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());
        assertWaitForPageScaleFactor(1.0f);

        final ContentView view = getActivity().getActiveContentView();

        assertEquals(0, view.getContentViewCore().getNativeScrollXForTest());
        assertEquals(0, view.getContentViewCore().getNativeScrollYForTest());

        // Vertical fling
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                view.fling(System.currentTimeMillis(), 0, 0, 0, -1000);
            }
        });

        // There's no end-of-fling notification so we need to busy-wait.
        for (int i = 0; i < 500; ++i) {
          if (view.getContentViewCore().getNativeScrollYForTest() > 100) {
            break;
          }
          Thread.sleep(10);
        }

        assertEquals(0, view.getContentViewCore().getNativeScrollXForTest());
        assertTrue(view.getContentViewCore().getNativeScrollYForTest() > 100);

        // Horizontal fling
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                view.fling(System.currentTimeMillis(), 0, 0, -1000, 0);
            }
        });

        // There's no end-of-fling notification so we need to busy-wait.
        for (int i = 0; i < 500; ++i) {
          if (view.getContentViewCore().getNativeScrollXForTest() > 100) {
            break;
          }
          Thread.sleep(10);
        }

        assertTrue(view.getContentViewCore().getNativeScrollXForTest() > 100);
        assertTrue(view.getContentViewCore().getNativeScrollYForTest() > 100);
    }
}
