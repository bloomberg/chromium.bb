// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.FlakyTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.content.browser.NavigationEntry;
import org.chromium.content.browser.NavigationHistory;
import org.chromium.content.browser.test.util.HistoryUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Callable;

public class NavigationHistoryTest extends AwTestBase {

    private static final String PAGE_1_PATH = "/page1.html";
    private static final String PAGE_1_TITLE = "Page 1 Title";
    private static final String PAGE_2_PATH = "/page2.html";
    private static final String PAGE_2_TITLE = "Page 2 Title";
    private static final String PAGE_WITH_HASHTAG_REDIRECT_TITLE = "Page with hashtag";

    private TestWebServer mWebServer;
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mWebServer = new TestWebServer(false);
    }

    @Override
    public void tearDown() throws Exception {
        mWebServer.shutdown();
        super.tearDown();
    }

    private NavigationHistory getNavigationHistory(final AwContents awContents)
            throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<NavigationHistory>() {
            @Override
            public NavigationHistory call() {
                return awContents.getContentViewCore().getNavigationHistory();
            }
        });
    }

    private void checkHistoryItem(NavigationEntry item, String url, String originalUrl,
            String title, boolean faviconNull) {
        assertEquals(url, item.getUrl());
        assertEquals(originalUrl, item.getOriginalUrl());
        assertEquals(title, item.getTitle());
        if (faviconNull) {
            assertNull(item.getFavicon());
        } else {
            assertNotNull(item.getFavicon());
        }
    }

    private String addPage1ToServer(TestWebServer webServer) {
        return mWebServer.setResponse(PAGE_1_PATH,
                CommonResources.makeHtmlPageFrom(
                        "<title>" + PAGE_1_TITLE + "</title>",
                        "<div>This is test page 1.</div>"),
                CommonResources.getTextHtmlHeaders(false));
    }

    private String addPage2ToServer(TestWebServer webServer) {
        return mWebServer.setResponse(PAGE_2_PATH,
                CommonResources.makeHtmlPageFrom(
                        "<title>" + PAGE_2_TITLE + "</title>",
                        "<div>This is test page 2.</div>"),
                CommonResources.getTextHtmlHeaders(false));
    }

    private String addPageWithHashTagRedirectToServer(TestWebServer webServer) {
        return mWebServer.setResponse(PAGE_2_PATH,
                CommonResources.makeHtmlPageFrom(
                        "<title>" + PAGE_WITH_HASHTAG_REDIRECT_TITLE + "</title>",
                        "<iframe onLoad=\"location.replace(location.href + '#tag');\" />"),
                CommonResources.getTextHtmlHeaders(false));
    }

    @SmallTest
    public void testNavigateOneUrl() throws Throwable {
        NavigationHistory history = getNavigationHistory(mAwContents);
        assertEquals(0, history.getEntryCount());

        final String pageWithHashTagRedirectUrl = addPageWithHashTagRedirectToServer(mWebServer);
        enableJavaScriptOnUiThread(mAwContents);

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                pageWithHashTagRedirectUrl);

        history = getNavigationHistory(mAwContents);
        checkHistoryItem(history.getEntryAtIndex(0),
                pageWithHashTagRedirectUrl + "#tag",
                pageWithHashTagRedirectUrl,
                PAGE_WITH_HASHTAG_REDIRECT_TITLE,
                true);

        assertEquals(0, history.getCurrentEntryIndex());
    }

    @SmallTest
    public void testNavigateTwoUrls() throws Throwable {
        NavigationHistory list = getNavigationHistory(mAwContents);
        assertEquals(0, list.getEntryCount());

        final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        final String page1Url = addPage1ToServer(mWebServer);
        final String page2Url = addPage2ToServer(mWebServer);

        loadUrlSync(mAwContents, onPageFinishedHelper, page1Url);
        loadUrlSync(mAwContents, onPageFinishedHelper, page2Url);

        list = getNavigationHistory(mAwContents);

        // Make sure there is a new entry entry the list
        assertEquals(2, list.getEntryCount());

        // Make sure the first entry is still okay
        checkHistoryItem(list.getEntryAtIndex(0),
                page1Url,
                page1Url,
                PAGE_1_TITLE,
                true);

        // Make sure the second entry was added properly
        checkHistoryItem(list.getEntryAtIndex(1),
                page2Url,
                page2Url,
                PAGE_2_TITLE,
                true);

        assertEquals(1, list.getCurrentEntryIndex());

    }

    @SmallTest
    public void testNavigateTwoUrlsAndBack() throws Throwable {
        final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        NavigationHistory list = getNavigationHistory(mAwContents);
        assertEquals(0, list.getEntryCount());

        final String page1Url = addPage1ToServer(mWebServer);
        final String page2Url = addPage2ToServer(mWebServer);

        loadUrlSync(mAwContents, onPageFinishedHelper, page1Url);
        loadUrlSync(mAwContents, onPageFinishedHelper, page2Url);

        HistoryUtils.goBackSync(getInstrumentation(), mAwContents.getContentViewCore(),
                onPageFinishedHelper);
        list = getNavigationHistory(mAwContents);

        // Make sure the first entry is still okay
        checkHistoryItem(list.getEntryAtIndex(0),
                page1Url,
                page1Url,
                PAGE_1_TITLE,
                true);

        // Make sure the second entry is still okay
        checkHistoryItem(list.getEntryAtIndex(1),
                page2Url,
                page2Url,
                PAGE_2_TITLE,
                true);

        // Make sure the current index is back to 0
        assertEquals(0, list.getCurrentEntryIndex());
    }

    /**
     * Disabled until favicons are getting fetched when using ContentView.
     *
     * @SmallTest
     * @throws Throwable
     */
    @DisabledTest
    public void testFavicon() throws Throwable {
        NavigationHistory list = getNavigationHistory(mAwContents);

        mWebServer.setResponseBase64("/" + CommonResources.FAVICON_FILENAME,
                CommonResources.FAVICON_DATA_BASE64, CommonResources.getImagePngHeaders(false));
        final String url = mWebServer.setResponse("/favicon.html",
                CommonResources.FAVICON_STATIC_HTML, null);

        assertEquals(0, list.getEntryCount());
        getContentSettingsOnUiThread(mAwContents).setImagesEnabled(true);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        list = getNavigationHistory(mAwContents);

        // Make sure the first entry is still okay.
        checkHistoryItem(list.getEntryAtIndex(0), url, url, "", false);
    }
}
