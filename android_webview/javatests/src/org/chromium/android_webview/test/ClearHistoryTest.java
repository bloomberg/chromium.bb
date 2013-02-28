// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.HistoryUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;

/**
 * Tests for a wanted clearHistory method.
 */
public class ClearHistoryTest extends AndroidWebViewTestBase {

    private static final String[] URLS = new String[3];
    {
        for (int i = 0; i < URLS.length; i++) {
            URLS[i] = UrlUtils.encodeHtmlDataUri(
                    "<html><head></head><body>" + i + "</body></html>");
        }
    }

    /*
    @SmallTest
    @Feature({"History", "Main"})
    This test is only failing on JellyBean bots.
    See crbug.com/178762.
    */
    @DisabledTest
    public void testClearHistory() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final ContentViewCore contentViewCore = testContainerView.getContentViewCore();

        OnPageFinishedHelper onPageFinishedHelper = contentsClient.getOnPageFinishedHelper();
        for (int i = 0; i < 3; i++) {
            loadUrlSync(awContents, onPageFinishedHelper, URLS[i]);
        }

        HistoryUtils.goBackSync(getInstrumentation(), contentViewCore, onPageFinishedHelper);
        assertTrue("Should be able to go back",
                   HistoryUtils.canGoBackOnUiThread(getInstrumentation(), contentViewCore));
        assertTrue("Should be able to go forward",
                   HistoryUtils.canGoForwardOnUiThread(getInstrumentation(), contentViewCore));

        HistoryUtils.clearHistoryOnUiThread(getInstrumentation(), contentViewCore);
        assertFalse("Should not be able to go back",
                    HistoryUtils.canGoBackOnUiThread(getInstrumentation(), contentViewCore));
        assertFalse("Should not be able to go forward",
                    HistoryUtils.canGoForwardOnUiThread(getInstrumentation(), contentViewCore));
    }
}
