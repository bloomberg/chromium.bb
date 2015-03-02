// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Build;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.net.test.util.TestWebServer;

/**
 * Tests for pop up window flow.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT)
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
                popupPageHtml, "/popup.html", "tryOpenWindow()");
        AwContents popupContents = connectPendingPopup(mParentContents);
        assertEquals(POPUP_TITLE, getTitleOnUiThread(popupContents));
    }
}
