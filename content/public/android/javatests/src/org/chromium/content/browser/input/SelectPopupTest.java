// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.test.suitebuilder.annotation.LargeTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.concurrent.TimeUnit;

/**
 * Integration Tests for SelectPopup.
 */
public class SelectPopupTest extends ContentShellTestBase {
    private static final long WAIT_TIMEOUT_SECONDS = scaleTimeout(2);
    private static final String SELECT_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\"" +
            "content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" /></head>" +
            "<body>Which animal is the strongest:<br/>" +
            "<select id=\"select\">" +
            "<option>Black bear</option>" +
            "<option>Polar bear</option>" +
            "<option>Grizzly</option>" +
            "<option>Tiger</option>" +
            "<option>Lion</option>" +
            "<option>Gorilla</option>" +
            "<option>Chipmunk</option>" +
            "</select>" +
            "</body></html>");

    private class PopupShowingCriteria implements Criteria {
        @Override
        public boolean isSatisfied() {
           return getContentViewCore().getSelectPopupForTest() != null;
        }
    }

    private class PopupHiddenCriteria implements Criteria {
        @Override
        public boolean isSatisfied() {
            return getContentViewCore().getSelectPopupForTest() == null;
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        launchContentShellWithUrl(SELECT_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());
        // TODO(aurimas) remove this wait once crbug.com/179511 is fixed.
        assertWaitForPageScaleFactorMatch(1);
    }

    /**
     * Tests that showing a select popup and having the page reload while the popup is showing does
     * not assert.
     */
    @LargeTest
    @Feature({"Browser"})
    @RerunWithUpdatedContainerView
    public void testReloadWhilePopupShowing() throws InterruptedException, Exception, Throwable {
        // The popup should be hidden before the click.
        assertTrue("The select popup is shown after load.",
                CriteriaHelper.pollForCriteria(new PopupHiddenCriteria()));

        final ContentViewCore viewCore = getContentViewCore();
        final TestCallbackHelperContainer viewClient = new TestCallbackHelperContainer(viewCore);
        final OnPageFinishedHelper onPageFinishedHelper = viewClient.getOnPageFinishedHelper();

        // Once clicked, the popup should show up.
        DOMUtils.clickNode(this, viewCore, "select");
        assertTrue("The select popup did not show up on click.",
                CriteriaHelper.pollForCriteria(new PopupShowingCriteria()));

        // Reload the test page.
        int currentCallCount = onPageFinishedHelper.getCallCount();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Now reload the page while the popup is showing, it gets hidden.
                getContentViewCore().getWebContents().getNavigationController().reload(true);
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount, 1,
                WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);

        // The popup should be hidden after the page reload.
        assertTrue("The select popup did not hide after reload.",
                CriteriaHelper.pollForCriteria(new PopupHiddenCriteria()));

        // Click the select and wait for the popup to show.
        DOMUtils.clickNode(this, viewCore, "select");
        assertTrue("The select popup did not show on click after reload.",
                CriteriaHelper.pollForCriteria(new PopupShowingCriteria()));
    }
}
