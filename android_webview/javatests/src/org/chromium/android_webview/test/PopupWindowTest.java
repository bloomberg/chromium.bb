// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Callable;

/**
 * Tests for pop up window flow.
 */
public class PopupWindowTest extends AwTestBase {
    private TestAwContentsClient mParentContentsClient;
    private AwTestContainerView mParentContainerView;
    private AwContents mParentContents;
    private TestWebServer mWebServer;

    private static final String POPUP_TITLE = "Popup Window";

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mParentContentsClient = new TestAwContentsClient();
        mParentContainerView = createAwTestContainerViewOnMainSync(mParentContentsClient);
        mParentContents = mParentContainerView.getAwContents();
        mWebServer = TestWebServer.start();
    }

    @Override
    public void tearDown() throws Exception {
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
        super.tearDown();
    }

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

        triggerPopup(mParentContents, mParentContentsClient, mWebServer, parentPageHtml,
                popupPageHtml, popupPath, "tryOpenWindow()");
        AwContents popupContents = connectPendingPopup(mParentContents).popupContents;
        assertEquals(POPUP_TITLE, getTitleOnUiThread(popupContents));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @RetryOnFailure
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

        triggerPopup(mParentContents, mParentContentsClient, mWebServer, parentPageHtml,
                popupPageHtml, popupPath, "tryOpenWindow()");
        PopupInfo popupInfo = connectPendingPopup(mParentContents);
        assertEquals(POPUP_TITLE, getTitleOnUiThread(popupInfo.popupContents));

        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                popupInfo.popupContentsClient.getOnPageFinishedHelper();
        final int onPageFinishedCallCount = onPageFinishedHelper.getCallCount();

        executeJavaScriptAndWaitForResult(mParentContents, mParentContentsClient,
                "modifyDomOfPopup()");
        // Test that |waitForCallback| does not time out.
        onPageFinishedHelper.waitForCallback(onPageFinishedCallCount);
    }

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

        triggerPopup(mParentContents, mParentContentsClient, mWebServer, parentPageHtml,
                popupPageHtml, popupPath, "tryOpenWindow()");
        PopupInfo popupInfo = connectPendingPopup(mParentContents);
        final AwContents popupContents = popupInfo.popupContents;
        TestAwContentsClient popupContentsClient = popupInfo.popupContentsClient;
        assertEquals(POPUP_TITLE, getTitleOnUiThread(popupContents));

        enableJavaScriptOnUiThread(popupContents);

        // Now long press on some texts and see if the text handles show up.
        DOMUtils.longPressNode(popupContents.getContentViewCore(), "plain_text");
        assertWaitForSelectActionBarStatus(true, popupContents.getContentViewCore());
        assertTrue(runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return popupContents.getContentViewCore()
                        .getSelectionPopupControllerForTesting().hasSelection();
            }
        }));

        // Now hide the select action bar. This should hide the text handles and
        // clear the selection.
        hideSelectActionMode(popupContents.getContentViewCore());

        assertWaitForSelectActionBarStatus(false, popupContents.getContentViewCore());
        String jsGetSelection = "window.getSelection().toString()";
        // Test window.getSelection() returns empty string "" literally.
        assertEquals("\"\"", executeJavaScriptAndWaitForResult(
                                     popupContents, popupContentsClient, jsGetSelection));
    }

    // Copied from imeTest.java.
    private void assertWaitForSelectActionBarStatus(boolean show, final ContentViewCore cvc) {
        CriteriaHelper.pollUiThread(Criteria.equals(show, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return cvc.isSelectActionBarShowing();
            }
        }));
    }

    private void hideSelectActionMode(final ContentViewCore cvc) {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                cvc.destroySelectActionMode();
            }
        });
    }
}
