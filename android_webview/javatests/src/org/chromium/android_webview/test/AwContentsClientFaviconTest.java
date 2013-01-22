// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.net.test.util.TestWebServer;

import java.io.InputStream;
import java.net.URL;

/**
 * Tests for the Favicon and TouchIcon related APIs.
 */
public class AwContentsClientFaviconTest extends AndroidWebViewTestBase {

    private static final String FAVICON1_URL = "/favicon1.png";
    private static final String FAVICON1_PAGE_URL = "/favicon1.html";
    private static final String FAVICON1_PAGE_HTML =
      CommonResources.makeHtmlPageFrom(
        "<link rel=\"icon\" href=\""+ FAVICON1_URL + "\" />",
        "Body");

    private static class FaviconHelper extends CallbackHelper {
        private String mUrl;
        private Bitmap mIcon;
        private boolean mTouch;
        private boolean mPrecomposed;

        public void notifyCalled(String url, Bitmap icon, boolean touch, boolean precomposed) {
            mUrl = url;
            mPrecomposed = precomposed;
            mTouch = touch;
            mIcon = icon;
            super.notifyCalled();
        }
    }

    private static class TestAwContentsClient
            extends org.chromium.android_webview.test.TestAwContentsClient {
         private FaviconHelper mFaviconHelper = new FaviconHelper();

         @Override
         public void onReceivedIcon(Bitmap bitmap) {
             // We don't inform the API client about the URL of the icon.
             mFaviconHelper.notifyCalled(null, bitmap, false, false);
         }

         @Override
         public void onReceivedTouchIconUrl(String url, boolean precomposed) {
             mFaviconHelper.notifyCalled(url, null /* We don't download touch icons */,
               true, precomposed);
         }
    }

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mWebServer = new TestWebServer(false);
    }

    @SmallTest
    public void testRecieveBasicFavicon() throws Throwable {
        int callCount = mContentsClient.mFaviconHelper.getCallCount();

        final String faviconUrl = mWebServer.setResponseBase64(FAVICON1_URL,
          CommonResources.FAVICON_DATA_BASE64, CommonResources.getImagePngHeaders(true));
        final String pageUrl = mWebServer.setResponse(FAVICON1_PAGE_URL, FAVICON1_PAGE_HTML,
          CommonResources.getTextHtmlHeaders(true));

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);

        mContentsClient.mFaviconHelper.waitForCallback(callCount);
        assertNull(mContentsClient.mFaviconHelper.mUrl);
        assertFalse(mContentsClient.mFaviconHelper.mTouch);
        assertFalse(mContentsClient.mFaviconHelper.mPrecomposed);
        Object originalFaviconSource = (new URL(faviconUrl)).getContent();
        Bitmap originalFavicon = BitmapFactory.decodeStream((InputStream)originalFaviconSource);
        assertNotNull(originalFavicon);
        assertNotNull(mContentsClient.mFaviconHelper.mIcon);
        assertTrue(mContentsClient.mFaviconHelper.mIcon.sameAs(originalFavicon));
    }

    // TODO testTouchIcon. Not exactly working at the moment.
}
