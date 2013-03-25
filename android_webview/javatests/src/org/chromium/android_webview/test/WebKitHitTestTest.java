// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.res.Configuration;
import android.os.SystemClock;
import android.test.FlakyTest;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.webkit.WebView.HitTestResult;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Feature;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Callable;

public class WebKitHitTestTest extends AwTestBase {
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

    private void srcAnchorTypeTestBody(boolean byTouch) throws Throwable {
        String page = fullPageLink(HREF, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        assertTrue(pollForHitTestDataOnUiThread(HitTestResult.SRC_ANCHOR_TYPE, HREF));
        assertTrue(pollForHrefAndImageSrcOnUiThread(HREF, ANCHOR_TEXT, null));
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
        String fullpath = mWebServer.getResponseUrl("/hittest.html");
        String page = fullPageLink("", ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        assertTrue(pollForHitTestDataOnUiThread(
                HitTestResult.SRC_ANCHOR_TYPE, fullpath));
        assertTrue(pollForHrefAndImageSrcOnUiThread(null, ANCHOR_TEXT, null));
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
        String relpath = "/foo.html";
        String fullpath = mWebServer.getResponseUrl(relpath);
        String page = fullPageLink(relpath, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        assertTrue(pollForHitTestDataOnUiThread(
                HitTestResult.SRC_ANCHOR_TYPE, fullpath));
        assertTrue(pollForHrefAndImageSrcOnUiThread(relpath, ANCHOR_TEXT, null));
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
        assertTrue(pollForHitTestDataOnUiThread(HitTestResult.EMAIL_TYPE, email));
        assertTrue(pollForHrefAndImageSrcOnUiThread(prefix+ email, ANCHOR_TEXT, null));
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
        assertTrue(pollForHitTestDataOnUiThread(HitTestResult.GEO_TYPE, location));
        assertTrue(pollForHrefAndImageSrcOnUiThread(prefix + location, ANCHOR_TEXT, null));
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
        String phone_num = "1234567890";
        String prefix = "tel:";
        String page = fullPageLink("tel:" + phone_num, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        assertTrue(pollForHitTestDataOnUiThread(HitTestResult.PHONE_TYPE, phone_num));
        assertTrue(pollForHrefAndImageSrcOnUiThread(prefix + phone_num, ANCHOR_TEXT, null));
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
        String relImageSrc = "/nonexistent.jpg";
        String fullImageSrc = mWebServer.getResponseUrl(relImageSrc);
        String page = CommonResources.makeHtmlPageFrom("", "<a class=\"full_view\" href=\"" +
                HREF + "\"onclick=\"return false;\"><img class=\"full_view\" src=\"" +
                relImageSrc + "\"></a>");
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        assertTrue(pollForHitTestDataOnUiThread(
                HitTestResult.SRC_IMAGE_ANCHOR_TYPE, fullImageSrc));
        assertTrue(pollForHrefAndImageSrcOnUiThread(HREF, null, fullImageSrc));
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

    private void editTextTypeTestBody(boolean byTouch) throws Throwable {
        String page = CommonResources.makeHtmlPageFrom("",
                "<form><input class=\"full_view\" type=\"text\" name=\"test\"></form>");
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        assertTrue(pollForHitTestDataOnUiThread(
                HitTestResult.EDIT_TEXT_TYPE, null));
        assertTrue(pollForHrefAndImageSrcOnUiThread(null, null, null));
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
        assertTrue(pollForHrefAndImageSrcOnUiThread(javascript, ANCHOR_TEXT, null));
        assertTrue(pollForHitTestDataOnUiThread(HitTestResult.UNKNOWN_TYPE, null));
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
        assertTrue(pollForHitTestDataOnUiThread(HitTestResult.UNKNOWN_TYPE, null));
    }

    @LargeTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testUnfocusedNodeAndTouchRace() throws Throwable {
      // Test when the touch and focus paths racing with setting different
      // results.

      String relImageSrc = "/nonexistent3.jpg";
      String fullImageSrc = mWebServer.getResponseUrl(relImageSrc);
      String html = CommonResources.makeHtmlPageFrom(
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
      assertTrue(pollForHitTestDataOnUiThread(
              HitTestResult.EDIT_TEXT_TYPE, null));
      assertTrue(pollForHrefAndImageSrcOnUiThread(null, null, null));

      // Touch image. Now the focus based hit test path will try to null out
      // the results and the touch based path will update with the result of
      // the image.
      simulateTouchCenterOfWebViewOnUiThread();

      // Make sure the result of image sticks.
      for (int i = 0; i < 2; ++i) {
        Thread.sleep(500);
        assertTrue(pollForHitTestDataOnUiThread(
                HitTestResult.IMAGE_TYPE, fullImageSrc));
        assertTrue(pollForHrefAndImageSrcOnUiThread(null, null, fullImageSrc));
      }
    }
}
