// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.support.test.filters.LargeTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests for interstitial pages.
 */
public class InterstitialPageTest extends ContentShellTestBase {

    private static final String URL = UrlUtils.encodeHtmlDataUri(
            "<html><head></head><body>test</body></html>");

    private static class TestWebContentsObserver extends WebContentsObserver {
        private boolean mInterstitialShowing;

        public TestWebContentsObserver(WebContents webContents) {
            super(webContents);
        }

        public boolean isInterstitialShowing() throws ExecutionException {
            return ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() throws Exception {
                    return mInterstitialShowing;
                }
            }).booleanValue();
        }

        @Override
        public void didAttachInterstitialPage() {
            mInterstitialShowing = true;
        }

        @Override
        public void didDetachInterstitialPage() {
            mInterstitialShowing = false;
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        ContentShellActivity activity = launchContentShellWithUrl(URL);
        assertNotNull(activity);
        waitForActiveShellToBeDoneLoading();
    }

    private void waitForInterstitial(final boolean shouldBeShown) {
        CriteriaHelper.pollUiThread(
                Criteria.equals(shouldBeShown, new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return getWebContents().isShowingInterstitialPage();
                    }
                }));
    }

    /**
     * Tests that showing and hiding an interstitial works.
     */
    @LargeTest
    @Feature({"Navigation"})
    @RetryOnFailure
    public void testCloseInterstitial() throws ExecutionException {
        final String proceedCommand = "PROCEED";
        final String htmlContent = "<html>"
                + "<head>"
                + "  <script>"
                + "    function sendCommand(command) {"
                + "      window.domAutomationController.setAutomationId(1);"
                + "      window.domAutomationController.send(command);"
                + "    }"
                + "  </script>"
                + "</head>"
                + "<body style='background-color:#FF0000' "
                + "  onclick='sendCommand(\"" + proceedCommand + "\");'>"
                + "  <h1>This is a scary interstitial page</h1>"
                + "</body>"
                + "</html>";
        final InterstitialPageDelegateAndroid delegate =
                new InterstitialPageDelegateAndroid(htmlContent) {
            @Override
            protected void commandReceived(String command) {
                assertEquals(command, proceedCommand);
                proceed();
            }
        };
        TestWebContentsObserver observer = ThreadUtils.runOnUiThreadBlocking(
                new Callable<TestWebContentsObserver>() {
                    @Override
                    public TestWebContentsObserver call() throws Exception {
                        getWebContents().showInterstitialPage(URL, delegate.getNative());
                        return new TestWebContentsObserver(getWebContents());
                    }
                });

        waitForInterstitial(true);
        assertTrue("WebContentsObserver not notified of interstitial showing",
                observer.isInterstitialShowing());
        TouchCommon.singleClickView(getContentViewCore().getContainerView(), 10, 10);
        waitForInterstitial(false);
        assertTrue("WebContentsObserver not notified of interstitial hiding",
                !observer.isInterstitialShowing());
    }
}
