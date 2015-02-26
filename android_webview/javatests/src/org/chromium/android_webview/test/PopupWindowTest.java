// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Build;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.AwTestTouchUtils;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;

/**
 * Tests for pop up window flow.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT)
public class PopupWindowTest extends AwTestBase {
    private TestAwContentsClient mParentContentsClient;
    private AwTestContainerView mParentContainerView;
    private AwContents mParentContents;
    private TestAwContentsClient mPopupContentsClient;
    private AwTestContainerView mPopupContainerView;
    private AwContents mPopupContents;
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

    private void triggerPopup() throws Throwable {
        enableJavaScriptOnUiThread(mParentContents);
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mParentContents.getSettings().setSupportMultipleWindows(true);
                mParentContents.getSettings().setJavaScriptCanOpenWindowsAutomatically(true);
            }
        });

        final String popupPath = "/popup.html";

        final String parentPageHtml = CommonResources.makeHtmlPageFrom("",
                "<script>"
                        + "function tryOpenWindow() {"
                        + "  var newWindow = window.open('" + popupPath + "');"
                        + "}</script>"
                        + "<a class=\"full_view\" onclick=\"tryOpenWindow();\">Click me!</a>");
        final String popupPageHtml = CommonResources.makeHtmlPageFrom(
                "<title>" + POPUP_TITLE + "</title>",
                "This is a popup window");

        final String parentUrl = mWebServer.setResponse("/popupParent.html", parentPageHtml, null);
        mWebServer.setResponse(popupPath, popupPageHtml, null);

        mParentContentsClient.getOnCreateWindowHelper().setReturnValue(true);
        loadUrlSync(mParentContents,
                    mParentContentsClient.getOnPageFinishedHelper(),
                    parentUrl);

        TestAwContentsClient.OnCreateWindowHelper onCreateWindowHelper =
                mParentContentsClient.getOnCreateWindowHelper();
        int currentCallCount = onCreateWindowHelper.getCallCount();
        AwTestTouchUtils.simulateTouchCenterOfView(mParentContainerView);
        onCreateWindowHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
    }

    private void connectPendingPopup() throws Exception {
        mPopupContentsClient = new TestAwContentsClient();
        mPopupContainerView = createAwTestContainerViewOnMainSync(mPopupContentsClient);
        mPopupContents = mPopupContainerView.getAwContents();

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mParentContents.supplyContentsForPopup(mPopupContents);
            }
        });
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPopupWindow() throws Throwable {
        triggerPopup();
        connectPendingPopup();
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return POPUP_TITLE.equals(getTitleOnUiThread(mPopupContents));
            }
        });
    }
}
