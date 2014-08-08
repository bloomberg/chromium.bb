// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.webkit.WebView.HitTestResult;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Callable;

/**
 * Test for getHitTestResult, requestFocusNodeHref, and requestImageRef methods
 */
public class WebKitHitTestTest extends AwTestBase {
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    private static final String HREF = "http://foo/";
    private static final String ANCHOR_TEXT = "anchor text";

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
                float x = (float) (mTestView.getRight() - mTestView.getLeft()) / 2;
                float y = (float) (mTestView.getBottom() - mTestView.getTop()) / 2;
                mAwContents.onTouchEvent(MotionEvent.obtain(
                        eventTime, eventTime, MotionEvent.ACTION_DOWN,
                        x, y, 0));
                mAwContents.onTouchEvent(MotionEvent.obtain(
                        eventTime, eventTime, MotionEvent.ACTION_UP,
                        x, y, 0));
            }
        });
    }

    private void simulateTabDownUpOnUiThread() throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAwContents.getContentViewCore().dispatchKeyEvent(
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB));
                mAwContents.getContentViewCore().dispatchKeyEvent(
                        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_TAB));
            }
        });
    }

    private void simulateInput(boolean byTouch) throws Throwable {
        // Send a touch click event if byTouch is true. Otherwise, send a TAB
        // key event to change the focused element of the page.
        if (byTouch) {
            simulateTouchCenterOfWebViewOnUiThread();
        } else {
            simulateTabDownUpOnUiThread();
        }
    }

    private static boolean stringEquals(String a, String b) {
        return a == null ? b == null : a.equals(b);
    }

    private void pollForHitTestDataOnUiThread(
            final int expectedType, final String expectedExtra) throws Throwable {
        pollOnUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                AwContents.HitTestData data = mAwContents.getLastHitTestResult();
                return expectedType == data.hitTestResultType &&
                       stringEquals(expectedExtra, data.hitTestResultExtraData);
            }
        });
    }

    private void pollForHrefAndImageSrcOnUiThread(
            final String expectedHref,
            final String expectedAnchorText,
            final String expectedImageSrc) throws Throwable {
        pollOnUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                AwContents.HitTestData data = mAwContents.getLastHitTestResult();
                return stringEquals(expectedHref, data.href) &&
                       stringEquals(expectedAnchorText, data.anchorText) &&
                       stringEquals(expectedImageSrc, data.imgSrc);
            }
        });

        Handler dummyHandler = new Handler();
        final Message focusNodeHrefMsg = dummyHandler.obtainMessage();
        final Message imageRefMsg = dummyHandler.obtainMessage();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAwContents.requestFocusNodeHref(focusNodeHrefMsg);
                mAwContents.requestImageRef(imageRefMsg);
            }
        });
        assertEquals(expectedHref, focusNodeHrefMsg.getData().getString("url"));
        assertEquals(expectedAnchorText, focusNodeHrefMsg.getData().getString("title"));
        assertEquals(expectedImageSrc, focusNodeHrefMsg.getData().getString("src"));
        assertEquals(expectedImageSrc, imageRefMsg.getData().getString("url"));
    }

    private void srcAnchorTypeTestBody(boolean byTouch) throws Throwable {
        String page = fullPageLink(HREF, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.SRC_ANCHOR_TYPE, HREF);
        pollForHrefAndImageSrcOnUiThread(HREF, ANCHOR_TEXT, null);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcAnchorType() throws Throwable {
        srcAnchorTypeTestBody(true);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcAnchorTypeByFocus() throws Throwable {
        srcAnchorTypeTestBody(false);
    }

    private void blankHrefTestBody(boolean byTouch) throws Throwable {
        String fullPath = mWebServer.getResponseUrl("/hittest.html");
        String page = fullPageLink("", ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.SRC_ANCHOR_TYPE, fullPath);
        pollForHrefAndImageSrcOnUiThread(fullPath, ANCHOR_TEXT, null);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcAnchorTypeBlankHref() throws Throwable {
        blankHrefTestBody(true);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcAnchorTypeBlankHrefByFocus() throws Throwable {
        blankHrefTestBody(false);
    }

    private void srcAnchorTypeRelativeUrlTestBody(boolean byTouch) throws Throwable {
        String relPath = "/foo.html";
        String fullPath = mWebServer.getResponseUrl(relPath);
        String page = fullPageLink(relPath, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.SRC_ANCHOR_TYPE, fullPath);
        pollForHrefAndImageSrcOnUiThread(fullPath, ANCHOR_TEXT, null);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcAnchorTypeRelativeUrl() throws Throwable {
        srcAnchorTypeRelativeUrlTestBody(true);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcAnchorTypeRelativeUrlByFocus() throws Throwable {
        srcAnchorTypeRelativeUrlTestBody(false);
    }

    private void srcEmailTypeTestBody(boolean byTouch) throws Throwable {
        String email = "foo@bar.com";
        String prefix = "mailto:";
        String page = fullPageLink(prefix + email, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.EMAIL_TYPE, email);
        pollForHrefAndImageSrcOnUiThread(prefix + email, ANCHOR_TEXT, null);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcEmailType() throws Throwable {
        srcEmailTypeTestBody(true);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcEmailTypeByFocus() throws Throwable {
        srcEmailTypeTestBody(false);
    }

    private void srcGeoTypeTestBody(boolean byTouch) throws Throwable {
        String location = "Jilin";
        String prefix = "geo:0,0?q=";
        String page = fullPageLink(prefix + location, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.GEO_TYPE, location);
        pollForHrefAndImageSrcOnUiThread(prefix + location, ANCHOR_TEXT, null);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcGeoType() throws Throwable {
        srcGeoTypeTestBody(true);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcGeoTypeByFocus() throws Throwable {
        srcGeoTypeTestBody(false);
    }

    private void srcPhoneTypeTestBody(boolean byTouch) throws Throwable {
        String phone_num = "%2B1234567890";
        String expected_phone_num = "+1234567890";
        String prefix = "tel:";
        String page = fullPageLink("tel:" + phone_num, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.PHONE_TYPE, expected_phone_num);
        pollForHrefAndImageSrcOnUiThread(prefix + phone_num, ANCHOR_TEXT, null);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcPhoneType() throws Throwable {
        srcPhoneTypeTestBody(true);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcPhoneTypeByFocus() throws Throwable {
        srcPhoneTypeTestBody(false);
    }

    private void srcImgeAnchorTypeTestBody(boolean byTouch) throws Throwable {
        String fullImageSrc = "http://foo.bar/nonexistent.jpg";
        String page = CommonResources.makeHtmlPageFrom("", "<a class=\"full_view\" href=\"" +
                HREF + "\"onclick=\"return false;\"><img class=\"full_view\" src=\"" +
                fullImageSrc + "\"></a>");
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.SRC_IMAGE_ANCHOR_TYPE, fullImageSrc);
        pollForHrefAndImageSrcOnUiThread(HREF, null, fullImageSrc);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcImgeAnchorType() throws Throwable {
        srcImgeAnchorTypeTestBody(true);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcImgeAnchorTypeByFocus() throws Throwable {
        srcImgeAnchorTypeTestBody(false);
    }

    private void srcImgeAnchorTypeRelativeUrlTestBody(boolean byTouch) throws Throwable {
        String relImageSrc = "/nonexistent.jpg";
        String fullImageSrc = mWebServer.getResponseUrl(relImageSrc);
        String relPath = "/foo.html";
        String fullPath = mWebServer.getResponseUrl(relPath);
        String page = CommonResources.makeHtmlPageFrom("", "<a class=\"full_view\" href=\"" +
                relPath + "\"onclick=\"return false;\"><img class=\"full_view\" src=\"" +
                relImageSrc + "\"></a>");
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.SRC_IMAGE_ANCHOR_TYPE, fullImageSrc);
        pollForHrefAndImageSrcOnUiThread(fullPath, null, fullImageSrc);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcImgeAnchorTypeRelativeUrl() throws Throwable {
        srcImgeAnchorTypeRelativeUrlTestBody(true);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcImgeAnchorTypeRelativeUrlByFocus() throws Throwable {
        srcImgeAnchorTypeRelativeUrlTestBody(false);
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
        pollForHitTestDataOnUiThread(HitTestResult.IMAGE_TYPE, fullImageSrc);
        pollForHrefAndImageSrcOnUiThread(null, null, fullImageSrc);
    }

    private void editTextTypeTestBody(boolean byTouch) throws Throwable {
        String page = CommonResources.makeHtmlPageFrom("",
                "<form><input class=\"full_view\" type=\"text\" name=\"test\"></form>");
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.EDIT_TEXT_TYPE, null);
        pollForHrefAndImageSrcOnUiThread(null, null, null);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testEditTextType() throws Throwable {
        editTextTypeTestBody(true);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testEditTextTypeByFocus() throws Throwable {
        editTextTypeTestBody(false);
    }

    public void unknownTypeJavascriptSchemeTestBody(boolean byTouch) throws Throwable {
        // Per documentation, javascript urls are special.
        String javascript = "javascript:alert('foo');";
        String page = fullPageLink(javascript, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHrefAndImageSrcOnUiThread(javascript, ANCHOR_TEXT, null);
        pollForHitTestDataOnUiThread(HitTestResult.UNKNOWN_TYPE, null);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testUnknownTypeJavascriptScheme() throws Throwable {
        unknownTypeJavascriptSchemeTestBody(true);
    }

    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testUnknownTypeJavascriptSchemeByFocus() throws Throwable {
        unknownTypeJavascriptSchemeTestBody(false);
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
        pollForHitTestDataOnUiThread(HitTestResult.UNKNOWN_TYPE, null);
    }

    @LargeTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testUnfocusedNodeAndTouchRace() throws Throwable {
        // Test when the touch and focus paths racing with setting different
        // results.

        String relImageSrc = "/nonexistent3.jpg";
        String fullImageSrc = mWebServer.getResponseUrl(relImageSrc);
        String html = CommonResources.makeHtmlPageFrom(
                "<meta name=\"viewport\" content=\"width=device-width,height=device-height\" />" +
                        "<style type=\"text/css\">" +
                        ".full_width { width:100%; position:absolute; }" +
                        "</style>",
                        "<form><input class=\"full_width\" style=\"height:25%;\" " +
                        "type=\"text\" name=\"test\"></form>" +
                        "<img class=\"full_width\" style=\"height:50%;top:25%;\" " +
                        "src=\"" + relImageSrc + "\">");
        setServerResponseAndLoad(html);

        // Focus on input element and check the hit test results.
        simulateTabDownUpOnUiThread();
        pollForHitTestDataOnUiThread(HitTestResult.EDIT_TEXT_TYPE, null);
        pollForHrefAndImageSrcOnUiThread(null, null, null);

        // Touch image. Now the focus based hit test path will try to null out
        // the results and the touch based path will update with the result of
        // the image.
        simulateTouchCenterOfWebViewOnUiThread();

        // Make sure the result of image sticks.
        for (int i = 0; i < 2; ++i) {
            Thread.sleep(500);
            pollForHitTestDataOnUiThread(HitTestResult.IMAGE_TYPE, fullImageSrc);
            pollForHrefAndImageSrcOnUiThread(null, null, fullImageSrc);
        }
    }
}
