// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.AwActivityTestRule.PopupInfo;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;

/**
 * Tests for pop up window flow.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class PopupWindowTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mParentContentsClient;
    private AwTestContainerView mParentContainerView;
    private AwContents mParentContents;
    private TestWebServer mWebServer;

    private static final String POPUP_TITLE = "Popup Window";

    @Before
    public void setUp() throws Exception {
        mParentContentsClient = new TestAwContentsClient();
        mParentContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mParentContentsClient);
        mParentContents = mParentContainerView.getAwContents();
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() throws Exception {
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPopupWindow() throws Throwable {
        final String popupPath = "/popup.html";
        final String parentPageHtml = CommonResources.makeHtmlPageFrom("", "<script>"
                        + "function tryOpenWindow() {"
                        + "  var newWindow = window.open('" + popupPath + "');"
                        + "}</script>");

        final String popupPageHtml = CommonResources.makeHtmlPageFrom(
                "<title>" + POPUP_TITLE + "</title>",
                "This is a popup window");

        mActivityTestRule.triggerPopup(mParentContents, mParentContentsClient, mWebServer,
                parentPageHtml, popupPageHtml, popupPath, "tryOpenWindow()");
        AwContents popupContents =
                mActivityTestRule.connectPendingPopup(mParentContents).popupContents;
        Assert.assertEquals(POPUP_TITLE, mActivityTestRule.getTitleOnUiThread(popupContents));
    }

    @Test
    @DisabledTest
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinishedCalledOnDomModificationAfterNavigation() throws Throwable {
        final String popupPath = "/popup.html";
        final String parentPageHtml = CommonResources.makeHtmlPageFrom("", "<script>"
                        + "function tryOpenWindow() {"
                        + "  window.popupWindow = window.open('" + popupPath + "');"
                        + "}"
                        + "function modifyDomOfPopup() {"
                        + "  window.popupWindow.document.body.innerHTML = 'Hello from the parent!';"
                        + "}</script>");

        final String popupPageHtml = CommonResources.makeHtmlPageFrom(
                "<title>" + POPUP_TITLE + "</title>",
                "This is a popup window");

        mActivityTestRule.triggerPopup(mParentContents, mParentContentsClient, mWebServer,
                parentPageHtml, popupPageHtml, popupPath, "tryOpenWindow()");
        PopupInfo popupInfo = mActivityTestRule.connectPendingPopup(mParentContents);
        Assert.assertEquals(
                POPUP_TITLE, mActivityTestRule.getTitleOnUiThread(popupInfo.popupContents));

        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                popupInfo.popupContentsClient.getOnPageFinishedHelper();
        final int onPageFinishedCallCount = onPageFinishedHelper.getCallCount();

        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mParentContents, mParentContentsClient, "modifyDomOfPopup()");
        // Test that |waitForCallback| does not time out.
        onPageFinishedHelper.waitForCallback(onPageFinishedCallCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @RetryOnFailure
    public void testPopupWindowTextHandle() throws Throwable {
        final String popupPath = "/popup.html";
        final String parentPageHtml = CommonResources.makeHtmlPageFrom("", "<script>"
                        + "function tryOpenWindow() {"
                        + "  var newWindow = window.open('" + popupPath + "');"
                        + "}</script>");

        final String popupPageHtml = CommonResources.makeHtmlPageFrom(
                "<title>" + POPUP_TITLE + "</title>",
                "<span id=\"plain_text\" class=\"full_view\">This is a popup window.</span>");

        mActivityTestRule.triggerPopup(mParentContents, mParentContentsClient, mWebServer,
                parentPageHtml, popupPageHtml, popupPath, "tryOpenWindow()");
        PopupInfo popupInfo = mActivityTestRule.connectPendingPopup(mParentContents);
        final AwContents popupContents = popupInfo.popupContents;
        TestAwContentsClient popupContentsClient = popupInfo.popupContentsClient;
        Assert.assertEquals(POPUP_TITLE, mActivityTestRule.getTitleOnUiThread(popupContents));

        mActivityTestRule.enableJavaScriptOnUiThread(popupContents);

        // Now long press on some texts and see if the text handles show up.
        DOMUtils.longPressNode(popupContents.getContentViewCore(), "plain_text");
        assertWaitForSelectActionBarStatus(true, popupContents.getContentViewCore());
        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(() -> popupContents.getContentViewCore()
                    .getSelectionPopupControllerForTesting().hasSelection()));

        // Now hide the select action bar. This should hide the text handles and
        // clear the selection.
        hideSelectActionMode(popupContents.getContentViewCore());

        assertWaitForSelectActionBarStatus(false, popupContents.getContentViewCore());
        String jsGetSelection = "window.getSelection().toString()";
        // Test window.getSelection() returns empty string "" literally.
        Assert.assertEquals("\"\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        popupContents, popupContentsClient, jsGetSelection));
    }

    // Copied from imeTest.java.
    private void assertWaitForSelectActionBarStatus(boolean show, final ContentViewCore cvc) {
        CriteriaHelper.pollUiThread(Criteria.equals(show, () -> cvc.isSelectActionBarShowing()));
    }

    private void hideSelectActionMode(final ContentViewCore cvc) {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> cvc.destroySelectActionMode());
    }
}
