// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.SystemClock;
import android.test.FlakyTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.MotionEvent;
import android.webkit.WebView.HitTestResult;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Feature;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Callable;

public class WebKitHitTestTest extends AndroidWebViewTestBase {
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    private static String HREF = "http://foo/";
    private static String ANCHOR_TEXT = "anchor text";

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        mTestView = createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestView.getAwContents();
        mWebServer = new TestWebServer(false);
    }

    @Override
    public void tearDown() throws Exception {
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
        super.tearDown();
    }

    private void setServerResponseAndLoad(String response) throws Throwable {
      String url = mWebServer.setResponse("/hittest.html", response, null);
      loadUrlSync(mAwContents,
                  mContentsClient.getOnPageFinishedHelper(),
                  url);
    }

    private static String fullPageLink(String href, String anchorText) {
        return CommonResources.makeHtmlPageFrom("", "<a class=\"full_view\" href=\"" +
                href + "\" " + "onclick=\"return false;\">" + anchorText + "</a>");
    }

    private void simulateTouchCenterOfWebViewOnUiThread() throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                long eventTime = SystemClock.uptimeMillis();
                float x = (float)(mTestView.getRight() - mTestView.getLeft()) / 2;
                float y = (float)(mTestView.getBottom() - mTestView.getTop()) / 2;
                mAwContents.onTouchEvent(MotionEvent.obtain(
                        eventTime, eventTime, MotionEvent.ACTION_DOWN,
                        x, y, 0));
                mAwContents.onTouchEvent(MotionEvent.obtain(
                        eventTime, eventTime, MotionEvent.ACTION_UP,
                        x, y, 0));
            }
        });
    }

    private boolean pollForHitTestDataOnUiThread(
            final int type, final String extra) throws Throwable {
        return pollOnUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                AwContents.HitTestData data = mAwContents.getLastHitTestResult();
                return type == data.hitTestResultType &&
                       (extra == null ? data.hitTestResultExtraData == null :
                       extra.equals(data.hitTestResultExtraData));
            }
        });
    }

    private boolean pollForHrefAndImageSrcOnUiThread(
            final String href,
            final String anchorText,
            final String imageSrc) throws Throwable {
        return pollOnUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                AwContents.HitTestData data = mAwContents.getLastHitTestResult();
                return (href == null ? data.href == null :
                        href.equals(data.href)) &&
                       (anchorText == null ? data.anchorText == null :
                        anchorText.equals(data.anchorText)) &&
                       (imageSrc == null ? data.imgSrc == null :
                        imageSrc.equals(data.imgSrc));
            }
        });
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcAnchorType() throws Throwable {
        String page = fullPageLink(HREF, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateTouchCenterOfWebViewOnUiThread();
        assertTrue(pollForHitTestDataOnUiThread(HitTestResult.SRC_ANCHOR_TYPE, HREF));
        assertTrue(pollForHrefAndImageSrcOnUiThread(HREF, ANCHOR_TEXT, null));
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcAnchorTypeRelativeUrl() throws Throwable {
        String relpath = "/foo.html";
        String fullpath = mWebServer.getResponseUrl(relpath);
        String page = fullPageLink(relpath, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateTouchCenterOfWebViewOnUiThread();
        assertTrue(pollForHitTestDataOnUiThread(
                HitTestResult.SRC_ANCHOR_TYPE, fullpath));
        assertTrue(pollForHrefAndImageSrcOnUiThread(relpath, ANCHOR_TEXT, null));
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcEmailType() throws Throwable {
        String email = "foo@bar.com";
        String prefix = "mailto:";
        String page = fullPageLink(prefix + email, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateTouchCenterOfWebViewOnUiThread();
        assertTrue(pollForHitTestDataOnUiThread(HitTestResult.EMAIL_TYPE, email));
        assertTrue(pollForHrefAndImageSrcOnUiThread(prefix+ email, ANCHOR_TEXT, null));
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcGeoType() throws Throwable {
        String location = "Jilin";
        String prefix = "geo:0,0?q=";
        String page = fullPageLink(prefix + location, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateTouchCenterOfWebViewOnUiThread();
        assertTrue(pollForHitTestDataOnUiThread(HitTestResult.GEO_TYPE, location));
        assertTrue(pollForHrefAndImageSrcOnUiThread(prefix + location, ANCHOR_TEXT, null));
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcPhoneType() throws Throwable {
        String phone_num = "1234567890";
        String prefix = "tel:";
        String page = fullPageLink("tel:" + phone_num, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateTouchCenterOfWebViewOnUiThread();
        assertTrue(pollForHitTestDataOnUiThread(HitTestResult.PHONE_TYPE, phone_num));
        assertTrue(pollForHrefAndImageSrcOnUiThread(prefix + phone_num, ANCHOR_TEXT, null));
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcImgeAnchorType() throws Throwable {
        String relImageSrc = "/nonexistent.jpg";
        String fullImageSrc = mWebServer.getResponseUrl(relImageSrc);
        String page = CommonResources.makeHtmlPageFrom("", "<a class=\"full_view\" href=\"" +
                HREF + "\"onclick=\"return false;\"><img class=\"full_view\" src=\"" +
                relImageSrc + "\"></a>");
        setServerResponseAndLoad(page);
        simulateTouchCenterOfWebViewOnUiThread();
        assertTrue(pollForHitTestDataOnUiThread(
                HitTestResult.SRC_IMAGE_ANCHOR_TYPE, fullImageSrc));
        assertTrue(pollForHrefAndImageSrcOnUiThread(HREF, null, fullImageSrc));
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testImgeType() throws Throwable {
        String relImageSrc = "/nonexistent2.jpg";
        String fullImageSrc = mWebServer.getResponseUrl(relImageSrc);
        String page = CommonResources.makeHtmlPageFrom("",
                "<img class=\"full_view\" src=\"" + relImageSrc + "\">");
        setServerResponseAndLoad(page);
        simulateTouchCenterOfWebViewOnUiThread();
        assertTrue(pollForHitTestDataOnUiThread(
                HitTestResult.IMAGE_TYPE, fullImageSrc));
        assertTrue(pollForHrefAndImageSrcOnUiThread(null, null, fullImageSrc));
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testEditTextType() throws Throwable {
        String page = CommonResources.makeHtmlPageFrom("",
                "<form><input class=\"full_view\" type=\"text\" name=\"test\"></form>");
        setServerResponseAndLoad(page);
        simulateTouchCenterOfWebViewOnUiThread();
        assertTrue(pollForHitTestDataOnUiThread(
                HitTestResult.EDIT_TEXT_TYPE, null));
        assertTrue(pollForHrefAndImageSrcOnUiThread(null, null, null));
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testUnknownTypeJavascriptScheme() throws Throwable {
        // Per documentation, javascript urls are special.
        String javascript = "javascript:alert('foo');";
        String page = fullPageLink(javascript, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateTouchCenterOfWebViewOnUiThread();
        assertTrue(pollForHrefAndImageSrcOnUiThread(javascript, ANCHOR_TEXT, null));
        assertTrue(pollForHitTestDataOnUiThread(HitTestResult.UNKNOWN_TYPE, null));
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testUnknownTypeUnrecognizedNode() throws Throwable {
        // Since UNKNOWN_TYPE is the default, hit test another type first for
        // this test to be valid.
        testSrcAnchorType();

        final String title = "UNKNOWN_TYPE title";

        String page = CommonResources.makeHtmlPageFrom(
                "<title>" + title + "</title>",
                "<div class=\"full_view\">div text</div>");
        setServerResponseAndLoad(page);

        // Wait for the new page to be loaded before trying hit test.
        pollOnUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return mAwContents.getContentViewCore().getTitle().equals(title);
            }
        });
        simulateTouchCenterOfWebViewOnUiThread();
        assertTrue(pollForHitTestDataOnUiThread(HitTestResult.UNKNOWN_TYPE, null));
    }
}
