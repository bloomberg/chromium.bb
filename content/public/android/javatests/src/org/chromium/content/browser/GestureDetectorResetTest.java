// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.LargeTest;

import junit.framework.Assert;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.concurrent.TimeUnit;

public class GestureDetectorResetTest extends ContentShellTestBase {
    private static final int WAIT_TIMEOUT_SECONDS = 2;
    private static final String CLICK_TEST_URL = UrlUtils.encodeHtmlDataUri(
            "<html><body>" +
            "<button id=\"button\" " +
            "  onclick=\"document.getElementById('test').textContent = 'clicked';\">" +
            "Button" +
            "</button><br/>" +
            "<div id=\"test\">not clicked</div><br/>" +
            "</body></html>");

    private static class NodeContentsIsEqualToCriteria implements Criteria {
        private final ContentView mView;
        private final TestCallbackHelperContainer mViewClient;
        private final String mNodeId;
        private final String mExpectedContents;

        public NodeContentsIsEqualToCriteria(
                ContentView view,
                TestCallbackHelperContainer viewClient,
                String nodeId, String expectedContents) {
            mView = view;
            mViewClient = viewClient;
            mNodeId = nodeId;
            mExpectedContents = expectedContents;
            assert mExpectedContents != null;
        }

        @Override
        public boolean isSatisfied() {
            try {
                String contents = DOMUtils.getNodeContents(mView, mViewClient, mNodeId);
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
            ContentView view, TestCallbackHelperContainer viewClient)
                    throws InterruptedException, Exception, Throwable {
        // Initially the text on the page should say "not clicked".
        assertTrue("The page contents is invalid " + disambiguation,
                CriteriaHelper.pollForCriteria(new NodeContentsIsEqualToCriteria(
                        view, viewClient, "test", "not clicked")));

        // Click the button.
        DOMUtils.clickNode(this, view, viewClient, "button");

        // After the click, the text on the page should say "clicked".
        assertTrue("The page contents didn't change after a click " + disambiguation,
                CriteriaHelper.pollForCriteria(new NodeContentsIsEqualToCriteria(
                        view, viewClient, "test", "clicked")));
    }

    /**
     * Tests that showing a select popup and having the page reload while the popup is showing does
     * not assert.
     *
     * @LargeTest
     * @Feature({"Browser"})
     * BUG 172967
     */
    @DisabledTest
    public void testSeparateClicksAreRegisteredOnReload()
            throws InterruptedException, Exception, Throwable {
        // Load the test page.
        launchContentShellWithUrl(CLICK_TEST_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());

        final ContentView view = getActivity().getActiveContentView();
        final TestCallbackHelperContainer viewClient =
                new TestCallbackHelperContainer(view);
        final OnPageFinishedHelper onPageFinishedHelper =
                viewClient.getOnPageFinishedHelper();

        // Test that the button click works.
        verifyClicksAreRegistered("on initial load", view, viewClient);

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
        verifyClicksAreRegistered("after reload", view, viewClient);

        // Directly navigate to the test page.
        currentCallCount = onPageFinishedHelper.getCallCount();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                getActivity().getActiveShell().getContentView().loadUrl(
                        new LoadUrlParams(CLICK_TEST_URL));
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount, 1,
                WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);

        // Test that the button click still works.
        verifyClicksAreRegistered("after direct navigation", view, viewClient);
    }
}
