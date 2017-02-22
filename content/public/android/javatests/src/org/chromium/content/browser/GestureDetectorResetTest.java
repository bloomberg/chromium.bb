// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.support.test.filters.LargeTest;

import junit.framework.Assert;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.concurrent.TimeUnit;

/**
 * Provides test environment for Gesture Detector Reset for Content Shell.
 * This is a helper class for Content Shell tests.
*/
public class GestureDetectorResetTest extends ContentShellTestBase {
    private static final long WAIT_TIMEOUT_SECONDS = scaleTimeout(2);
    private static final String CLICK_TEST_URL = UrlUtils.encodeHtmlDataUri("<html><body>"
            + "<button id=\"button\" "
            + "  onclick=\"document.getElementById('test').textContent = 'clicked';\">"
            + "Button"
            + "</button><br/>"
            + "<div id=\"test\">not clicked</div><br/>"
            + "</body></html>");

    private static class NodeContentsIsEqualToCriteria extends Criteria {
        private final ContentViewCore mViewCore;
        private final String mNodeId;
        private final String mExpectedContents;

        public NodeContentsIsEqualToCriteria(
                String failureReason,
                ContentViewCore viewCore,
                String nodeId, String expectedContents) {
            super(failureReason);
            mViewCore = viewCore;
            mNodeId = nodeId;
            mExpectedContents = expectedContents;
            assert mExpectedContents != null;
        }

        @Override
        public boolean isSatisfied() {
            try {
                String contents = DOMUtils.getNodeContents(mViewCore.getWebContents(), mNodeId);
                return mExpectedContents.equals(contents);
            } catch (Throwable e) {
                Assert.fail("Failed to retrieve node contents: " + e);
                return false;
            }
        }
    }

    public GestureDetectorResetTest() {
    }

    private void verifyClicksAreRegistered(
            String disambiguation,
            ContentViewCore contentViewCore)
                    throws InterruptedException, Exception, Throwable {
        // Initially the text on the page should say "not clicked".
        CriteriaHelper.pollInstrumentationThread(new NodeContentsIsEqualToCriteria(
                "The page contents is invalid " + disambiguation, contentViewCore,
                "test", "not clicked"));

        // Click the button.
        DOMUtils.clickNode(contentViewCore, "button");

        // After the click, the text on the page should say "clicked".
        CriteriaHelper.pollInstrumentationThread(new NodeContentsIsEqualToCriteria(
                "The page contents didn't change after a click " + disambiguation,
                contentViewCore, "test", "clicked"));
    }

    /**
     * Tests that showing a select popup and having the page reload while the popup is showing does
     * not assert.
     */
    @LargeTest
    @Feature({"Browser"})
    @RetryOnFailure
    public void testSeparateClicksAreRegisteredOnReload()
            throws InterruptedException, Exception, Throwable {
        // Load the test page.
        launchContentShellWithUrl(CLICK_TEST_URL);
        waitForActiveShellToBeDoneLoading();

        final ContentViewCore viewCore = getContentViewCore();
        final TestCallbackHelperContainer viewClient =
                new TestCallbackHelperContainer(viewCore);
        final OnPageFinishedHelper onPageFinishedHelper =
                viewClient.getOnPageFinishedHelper();

        // Test that the button click works.
        verifyClicksAreRegistered("on initial load", viewCore);

        // Reload the test page.
        int currentCallCount = onPageFinishedHelper.getCallCount();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                getActivity().getActiveShell().loadUrl(CLICK_TEST_URL);
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount, 1,
                WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);

        // Test that the button click still works.
        verifyClicksAreRegistered("after reload", viewCore);

        // Directly navigate to the test page.
        currentCallCount = onPageFinishedHelper.getCallCount();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                getActivity().getActiveShell().getContentViewCore().getWebContents()
                        .getNavigationController().loadUrl(
                                new LoadUrlParams(CLICK_TEST_URL));
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount, 1,
                WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);

        // Test that the button click still works.
        verifyClicksAreRegistered("after direct navigation", viewCore);
    }
}
