// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.content.browser.test.util.HistoryUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Callable;

/**
 * Navigation history tests.
 */
public class NavigationHistoryTest extends AwTestBase {

    private static final String PAGE_1_PATH = "/page1.html";
    private static final String PAGE_1_TITLE = "Page 1 Title";
    private static final String PAGE_2_PATH = "/page2.html";
    private static final String PAGE_2_TITLE = "Page 2 Title";
    private static final String PAGE_WITH_HASHTAG_REDIRECT_TITLE = "Page with hashtag";
    private static final String LOGIN_PAGE_PATH = "/login.html";
    private static final String LOGIN_PAGE_TITLE = "Login page";
    private static final String LOGIN_RESPONSE_PAGE_PATH = "/login-response.html";
    private static final String LOGIN_RESPONSE_PAGE_TITLE = "Login response";
    private static final String LOGIN_RESPONSE_PAGE_HELP_LINK_ID = "help";

    private TestWebServer mWebServer;
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        AwContents.setShouldDownloadFavicons();
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
                return awContents.getNavigationController().getNavigationHistory();
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

        HistoryUtils.goBackSync(getInstrumentation(), mAwContents.getWebContents(),
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
        getAwSettingsOnUiThread(mAwContents).setImagesEnabled(true);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        list = getNavigationHistory(mAwContents);

        // Make sure the first entry is still okay.
        checkHistoryItem(list.getEntryAtIndex(0), url, url, "", false);
    }

    private String addNoncacheableLoginPageToServer(TestWebServer webServer) {
        final String submitButtonId = "submit";
        final String loginPageHtml =
                "<html>" +
                "  <head>" +
                "    <title>" + LOGIN_PAGE_TITLE + "</title>" +
                "    <script>" +
                "      function startAction() {" +
                "        button = document.getElementById('" + submitButtonId + "');" +
                "        button.click();" +
                "      }" +
                "    </script>" +
                "  </head>" +
                "  <body onload='setTimeout(startAction, 0)'>" +
                "    <form action='" + LOGIN_RESPONSE_PAGE_PATH.substring(1) + "' method='post'>" +
                "      <input type='text' name='login'>" +
                "      <input id='" + submitButtonId + "' type='submit' value='Submit'>" +
                "    </form>" +
                "  </body>" +
                "</html>";
        return mWebServer.setResponse(LOGIN_PAGE_PATH,
                loginPageHtml,
                CommonResources.getTextHtmlHeaders(true));
    }

    private String addNoncacheableLoginResponsePageToServer(TestWebServer webServer) {
        final String loginResponsePageHtml =
                "<html>" +
                "  <head>" +
                "    <title>" + LOGIN_RESPONSE_PAGE_TITLE + "</title>" +
                "  </head>" +
                "  <body>" +
                "    Login incorrect" +
                "    <div><a id='" + LOGIN_RESPONSE_PAGE_HELP_LINK_ID + "' href='" +
                PAGE_1_PATH.substring(1) + "'>Help</a></div>'" +
                "  </body>" +
                "</html>";
        return mWebServer.setResponse(LOGIN_RESPONSE_PAGE_PATH,
                loginResponsePageHtml,
                CommonResources.getTextHtmlHeaders(true));
    }

    // This test simulates Google login page behavior. The page is non-cacheable
    // and uses POST method for submission. It also contains a help link, leading
    // to another page. We are verifying that it is possible to go back to the
    // submitted login page after visiting the help page.
    /**
     * Temporarily disabled. It is blocking a patch that fixes chromium's form
     * resubmission defenses. This test should probably expect a modal dialog
     * asking permission to re-post rather than expecting to just be able to navigate
     * back to a page that specified Cache-Control: no-store.
     *
     * @MediumTest
     * @Feature({"AndroidWebView"})
     */
    @DisabledTest
    public void testNavigateBackToNoncacheableLoginPage() throws Throwable {
        final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        final String loginPageUrl = addNoncacheableLoginPageToServer(mWebServer);
        final String loginResponsePageUrl = addNoncacheableLoginResponsePageToServer(mWebServer);
        final String page1Url = addPage1ToServer(mWebServer);

        getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
        loadUrlSync(mAwContents, onPageFinishedHelper, loginPageUrl);
        // Since the page performs an async action, we can't rely on callbacks.
        pollOnUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                String title = mAwContents.getTitle();
                return LOGIN_RESPONSE_PAGE_TITLE.equals(title);
            }
        });
        executeJavaScriptAndWaitForResult(mAwContents,
                mContentsClient,
                "link = document.getElementById('" + LOGIN_RESPONSE_PAGE_HELP_LINK_ID + "');" +
                "link.click();");
        pollOnUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                String title = mAwContents.getTitle();
                return PAGE_1_TITLE.equals(title);
            }
        });
        // Verify that we can still go back to the login response page despite that
        // it is non-cacheable.
        HistoryUtils.goBackSync(getInstrumentation(), mAwContents.getWebContents(),
                onPageFinishedHelper);
        pollOnUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                String title = mAwContents.getTitle();
                return LOGIN_RESPONSE_PAGE_TITLE.equals(title);
            }
        });
    }
}
