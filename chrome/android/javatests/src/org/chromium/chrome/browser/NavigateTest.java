// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_TABLET;

import android.test.FlakyTest;
import android.test.suitebuilder.annotation.MediumTest;

import com.google.android.apps.chrome.R;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content.browser.test.util.UiUtils;
import org.chromium.net.test.util.TestWebServer;

import java.net.URLEncoder;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Navigate in UrlBar tests.
 */
public class NavigateTest extends ChromeTabbedActivityTestBase {
    private static final String HTTP_SCHEME = "http://";
    private static final String NEW_TAB_PAGE = "chrome-native://newtab/";

    /**
     * @return the expected contents of the location bar after navigating to url.
     */
    private String expectedLocation(String url) {
        assertTrue(url.startsWith(HTTP_SCHEME));
        return url.replaceFirst(HTTP_SCHEME, "");
    }

    /**
     * Verify Selection on the Location Bar
     * Bug: crbug.com/413277
     * @Smoke
     * @MediumTest
     * @Feature({"Navigation", "Main"})
     */
    @FlakyTest
    public void testNavigate() throws InterruptedException {
        String url = TestHttpServerClient.getUrl("chrome/test/data/android/navigate/simple.html");
        String result = typeInOmniboxAndNavigate(url);
        assertEquals(expectedLocation(url), result.trim());
    }

    @Restriction(RESTRICTION_TYPE_TABLET)
    @MediumTest
    @Feature({"Navigation"})
    public void testNavigateMany() throws InterruptedException {
        final String[] urls = {
                TestHttpServerClient.getUrl("chrome/test/data/android/navigate/one.html"),
                TestHttpServerClient.getUrl("chrome/test/data/android/navigate/two.html"),
                TestHttpServerClient.getUrl("chrome/test/data/android/navigate/three.html")
        };
        final int repeats = 3;
        for (int i = 0; i < repeats; i++) {
            for (int j = 0; j < urls.length; j++) {
                String result = typeInOmniboxAndNavigate(urls[j]);
                assertEquals(expectedLocation(urls[j]), result);
            }
        }
    }

    /**
     * Verify Selection on the Location Bar in Landscape Mode
     */
    @MediumTest
    @Feature({"Navigation"})
    public void testNavigateLandscape() throws InterruptedException {
        // '0' is Landscape Mode. '1' is Portrait.
        getActivity().setRequestedOrientation(0);
        String url = TestHttpServerClient.getUrl("chrome/test/data/android/navigate/simple.html");
        String result = typeInOmniboxAndNavigate(url);
        assertEquals(expectedLocation(url), result);
    }

    /**
     * Verify New Tab Open and Navigate.
     */
    @MediumTest
    @Feature({"Navigation"})
    public void testOpenAndNavigate() throws InterruptedException {
        final int tabCount = getActivity().getCurrentTabModel().getCount();
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        UiUtils.settleDownUI(getInstrumentation());

        assertEquals("Wrong number of tabs",
                tabCount + 1, getActivity().getCurrentTabModel().getCount());
        String url = TestHttpServerClient.getUrl("chrome/test/data/android/navigate/simple.html");
        String result = typeInOmniboxAndNavigate(url);
        assertEquals(expectedLocation(url), result);
    }

    /**
     * Test Opening a link and verify that the desired page is loaded.
     * Bug: crbug.com/171037
     * @MediumTest
     * @Feature({"Navigation"})
    */
    @FlakyTest
    public void testOpenLink() throws InterruptedException, TimeoutException {
        String url = TestHttpServerClient.getUrl("chrome/test/data/android/google.html");
        typeInOmniboxAndNavigate(url);
        assertEquals(url, getActivity().getActivityTab().getUrl());

        Tab tab = getActivity().getActivityTab();
        DOMUtils.clickNode(this, tab.getContentViewCore(), "aboutLink");
        ChromeTabUtils.waitForTabPageLoaded(
                tab, TestHttpServerClient.getUrl("chrome/test/data/android/about.html"));

        String expectedUrl = TestHttpServerClient.getUrl("chrome/test/data/android/about.html");
        assertEquals("Desired Link not open", expectedUrl,
                getActivity().getActivityTab().getUrl());
    }

    /**
     * Test Opening a link and verify that TabObserver#onPageLoadStarted gives the old and new URL.
     * Bug: crbug.com/171037
     * @MediumTest
     * @Feature({"Navigation"})
     */
    @FlakyTest
    public void testTabObserverOnPageLoadStarted() throws InterruptedException, TimeoutException {
        final String url1 = TestHttpServerClient.getUrl("chrome/test/data/android/google.html");
        final String url2 = TestHttpServerClient.getUrl("chrome/test/data/android/about.html");

        typeInOmniboxAndNavigate(url1);
        assertEquals(url1, getActivity().getActivityTab().getUrl());
        TabObserver onPageLoadStartedObserver = new EmptyTabObserver() {
            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                tab.removeObserver(this);
                assertEquals(tab.getUrl(), url1);
                assertEquals(url, url2);
            }
        };
        Tab tab = getActivity().getActivityTab();
        tab.addObserver(onPageLoadStartedObserver);
        DOMUtils.clickNode(this, tab.getContentViewCore(), "aboutLink");
        ChromeTabUtils.waitForTabPageLoaded(tab, url2);
        assertEquals("Desired Link not open", url2, getActivity().getActivityTab().getUrl());
    }

    /**
     * Test re-direct functionality for a web-page.
     * @throws Exception
     */
    @MediumTest
    @Feature({"Navigation"})
    public void testNavigateRedirect() throws Exception {
        final String initialUrl =
                TestHttpServerClient.getUrl("chrome/test/data/android/redirect/about.html");
        final String redirectedUrl =
                TestHttpServerClient.getUrl("chrome/test/data/android/redirect/one.html");
        typeInOmniboxAndNavigate(initialUrl);

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getActivity().getActivityTab().getUrl().equals(redirectedUrl);
            }
        }));
    }

    /**
     * Test fallback works as intended and that we can go back to the original URL
     * even when redirected via Java redirection.
     */
    @MediumTest
    @Feature({"Navigation"})
    public void testIntentFallbackRedirection() throws Exception {
        getInstrumentation().waitForIdleSync();
        assertEquals(NEW_TAB_PAGE, getActivity().getActivityTab().getUrl());

        final String redirectionUrl =
                TestHttpServerClient.getUrl("chrome/test/data/android/redirect/about.html");
        final String initialUrl = TestHttpServerClient.getUrl(
                "chrome/test/data/android/redirect/js_redirect.cgi?location="
                + URLEncoder.encode("intent://non_existent/#Intent;scheme=non_existent;"
                + "S.browser_fallback_url=" + redirectionUrl + ";end",
                "UTF-8"));
        final String targetUrl =
                TestHttpServerClient.getUrl("chrome/test/data/android/redirect/one.html");
        typeInOmniboxAndNavigate(initialUrl);

        // Now intent fallback should be triggered assuming 'non_existent' scheme cannot be handled.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getActivity().getActivityTab().getUrl().equals(targetUrl);
            }
        }));

        // Check if Java redirections were removed from the history.
        // Note that if we try to go back in the test: NavigateToEntry() is called, but
        // DidNavigate() does not get called. But in real cases we can go back to initial page
        // without any problem.
        // TODO(changwan): figure out why we cannot go back on this test.
        int index = getActivity().getActivityTab().getWebContents().getNavigationController()
                .getLastCommittedEntryIndex();
        assertEquals(1, index);
        String previousNavigationUrl = getActivity().getActivityTab().getWebContents()
                .getNavigationController().getEntryAtIndex(0).getUrl();
        assertEquals(NEW_TAB_PAGE, previousNavigationUrl);
    }

    /**
     * Test back and forward buttons.
     *
     * crbug.com/268463
     * @Restriction(RESTRICTION_TYPE_TABLET)
     * @MediumTest
     * @Feature({"Navigation"})
     */
    @FlakyTest
    public void testNavigateBackAndForwardButtons() throws InterruptedException {
        final String[] urls = {
                TestHttpServerClient.getUrl("chrome/test/data/android/navigate/one.html"),
                TestHttpServerClient.getUrl("chrome/test/data/android/navigate/two.html"),
                TestHttpServerClient.getUrl("chrome/test/data/android/navigate/three.html")
        };

        for (String url : urls) {
            typeInOmniboxAndNavigate(url);
        }

        final int repeats = 3;
        for (int i = 0; i < repeats; i++) {
            singleClickView(getActivity().findViewById(R.id.back_button));
            UiUtils.settleDownUI(getInstrumentation());
            assertEquals(String.format(
                    "URL mismatch after pressing back button for the 1st time in repetition %d.",
                    i), urls[1], getActivity().getActivityTab().getUrl());

            singleClickView(getActivity().findViewById(R.id.back_button));
            UiUtils.settleDownUI(getInstrumentation());
            assertEquals(String.format(
                    "URL mismatch after pressing back button for the 2nd time in repetition %d.",
                    i), urls[0], getActivity().getActivityTab().getUrl());

            singleClickView(getActivity().findViewById(R.id.forward_button));
            UiUtils.settleDownUI(getInstrumentation());
            assertEquals(String.format(
                    "URL mismatch after pressing fwd button for the 1st time in repetition %d.", i),
                    urls[1], getActivity().getActivityTab().getUrl());

            singleClickView(getActivity().findViewById(R.id.forward_button));
            UiUtils.settleDownUI(getInstrumentation());
            assertEquals(String.format(
                    "URL mismatch after pressing fwd button for the 2nd time in repetition %d.", i),
                    urls[2], getActivity().getActivityTab().getUrl());
        }
    }

    /*
     * Bug: crbug.com/413186
     * @MediumTest
     * @Feature({"Navigation"})
     */
    @FlakyTest
    public void testWindowOpenUrlSpoof() throws Exception {
        TestWebServer webServer = TestWebServer.start();
        try {
            // Make sure that we start with one tab.
            final TabModel model = getActivity().getTabModelSelector().getModel(false);
            assertEquals("Invalid starting number of tabs", 1, model.getCount());

            final Semaphore urlServedSemaphore = new Semaphore(0);
            Runnable checkAction = new Runnable() {
                @Override
                public void run() {
                    final Tab tab = TabModelUtils.getCurrentTab(model);

                    // Make sure that we are showing the spoofed data and a blank URL.
                    String url = getTabUrlOnUIThread(tab);
                    boolean spoofedUrl = "".equals(url) || "about:blank".equals(url);
                    assertTrue("URL Spoofed", spoofedUrl);
                    assertEquals("Not showing mocked content", "\"Spoofed\"", getTabBodyText(tab));
                    urlServedSemaphore.release();
                }
            };

            // Mock out the test URL
            final String mockedUrl = webServer.setResponseWithRunnableAction(
                    "/mockme.html", "<html><body>Real</body></html>", null, checkAction);

            // Navigate to the spoofable URL
            loadUrl(UrlUtils.encodeHtmlDataUri(
                      "<head>"
                    + "  <meta name=\"viewport\""
                    + "      content=\"initial-scale=1,maximum-scale=1,user-scalable=no\">"
                    + "</head>"
                    + "<script>"
                    + "  function spoof() {"
                    + "    var w = open();"
                    + "    w.opener = null;"
                    + "    w.document.write('Spoofed');"
                    + "    w.location = '" + mockedUrl + "'"
                    + "  }"
                    + "</script>"
                    + "<a href='JavaScript:spoof()' id='link'>Go</a>"));

            assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return getActivity().getCurrentContentViewCore().getScale() == 1.f;
                }
            }));

            // Click the 'Go' link
            DOMUtils.clickNode(this, getActivity().getCurrentContentViewCore(), "link");

            // Wait for the proper URL to be served.
            assertTrue(urlServedSemaphore.tryAcquire(5, TimeUnit.SECONDS));

            final Tab tab = TabModelUtils.getCurrentTab(model);

            // Wait for the url to change.
            assertTrue("Page url didn't change",
                    CriteriaHelper.pollForCriteria(new Criteria() {
                        @Override
                        public boolean isSatisfied() {
                            return mockedUrl.equals(getTabUrlOnUIThread(tab));
                        }
                    }, 5000, 50));

            // Make sure that we're showing new content now.
            assertEquals("Still showing spoofed data", "\"Real\"", getTabBodyText(tab));
        } finally {
            webServer.shutdown();
        }
    }

    private String getTabUrlOnUIThread(final Tab tab) {
        try {
            return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
                @Override
                public String call() throws Exception {
                    return tab.getUrl();
                }
            });
        } catch (ExecutionException ex) {
            assert false : "Unexpected ExecutionException";
        }
        return null;
    }

    private String getTabBodyText(Tab tab) {
        try {
            return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    tab.getWebContents(), "document.body.innerText");
        } catch (InterruptedException ex) {
            assert false : "Unexpected InterruptedException";
        } catch (TimeoutException ex) {
            assert false : "Unexpected TimeoutException";
        }
        return null;
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityFromLauncher();
    }
}
