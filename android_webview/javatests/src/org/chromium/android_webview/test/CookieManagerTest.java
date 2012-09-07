// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.MoreAsserts;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Pair;

import org.chromium.android_webview.CookieManager;
import org.chromium.base.test.Feature;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.Criteria;
import org.chromium.content.browser.test.CriteriaHelper;
import org.chromium.content.browser.test.TestContentViewClient.OnEvaluateJavaScriptResultHelper;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Tests for the CookieManager.
 */
public class CookieManagerTest extends AndroidWebViewTestBase {

    private CookieManager mCookieManager;
    private TestAwContentsClient mContentViewClient;
    private ContentViewCore mContentViewCore;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mCookieManager = new CookieManager();
        mContentViewClient = new TestAwContentsClient();
        mContentViewCore =
                createAwTestContainerViewOnMainSync(mContentViewClient).getContentViewCore();
        mContentViewCore.getContentSettings().setJavaScriptEnabled(true);
        assertNotNull(mCookieManager);
    }

    @SmallTest
    @Feature({"Android-WebView", "Privacy"})
    public void testAllowFileSchemeCookies() throws Throwable {
        assertFalse(CookieManager.allowFileSchemeCookies());
        CookieManager.setAcceptFileSchemeCookies(true);
        assertTrue(CookieManager.allowFileSchemeCookies());
        CookieManager.setAcceptFileSchemeCookies(false);
        assertFalse(CookieManager.allowFileSchemeCookies());
    }

    @MediumTest
    @Feature({"Android-WebView", "Privacy"})
    public void testAcceptCookie() throws Throwable {
        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            String path = "/cookie_test.html";
            String responseStr =
                    "<html><head><title>TEST!</title></head><body>HELLO!</body></html>";
            String url = webServer.setResponse(path, responseStr, null);

            mCookieManager.setAcceptCookie(false);
            mCookieManager.removeAllCookie();
            assertFalse(mCookieManager.acceptCookie());
            assertFalse(mCookieManager.hasCookies());

            loadUrlSync(mContentViewCore, mContentViewClient.getOnPageFinishedHelper(), url);
            setCookie("test1", "value1");
            assertNull(mCookieManager.getCookie(url));

            List<Pair<String, String>> responseHeaders = new ArrayList<Pair<String, String>>();
            responseHeaders.add(
                    Pair.create("Set-Cookie", "header-test1=header-value1; path=" + path));
            url = webServer.setResponse(path, responseStr, responseHeaders);
            loadUrlSync(mContentViewCore, mContentViewClient.getOnPageFinishedHelper(), url);
            assertNull(mCookieManager.getCookie(url));

            mCookieManager.setAcceptCookie(true);
            assertTrue(mCookieManager.acceptCookie());

            url = webServer.setResponse(path, responseStr, null);
            loadUrlSync(mContentViewCore, mContentViewClient.getOnPageFinishedHelper(), url);
            setCookie("test2", "value2");
            waitForCookie(url);
            String cookie = mCookieManager.getCookie(url);
            assertNotNull(cookie);
            validateCookies(cookie, "test2");

            responseHeaders = new ArrayList<Pair<String, String>>();
            responseHeaders.add(
                    Pair.create("Set-Cookie", "header-test2=header-value2 path=" + path));
            url = webServer.setResponse(path, responseStr, responseHeaders);
            loadUrlSync(mContentViewCore, mContentViewClient.getOnPageFinishedHelper(), url);
            waitForCookie(url);
            cookie = mCookieManager.getCookie(url);
            assertNotNull(cookie);
            validateCookies(cookie, "test2", "header-test2");

            // clean up all cookies
            mCookieManager.removeAllCookie();
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    private void setCookie(final String name, final String value)
            throws Throwable {
        OnEvaluateJavaScriptResultHelper onEvaluateJavaScriptResultHelper =
                mContentViewClient.getOnEvaluateJavaScriptResultHelper();
        int currentCallCount = onEvaluateJavaScriptResultHelper.getCallCount();
        final AtomicInteger requestId = new AtomicInteger();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                requestId.set(mContentViewCore.evaluateJavaScript(
                        "var expirationDate = new Date();" +
                        "expirationDate.setDate(expirationDate.getDate() + 5);" +
                        "document.cookie='" + name + "=" + value +
                                "; expires=' + expirationDate.toUTCString();"));
            }
        });
        onEvaluateJavaScriptResultHelper.waitForCallback(currentCallCount);
        assertEquals("Response ID mismatch when evaluating JavaScript.",
                requestId.get(), onEvaluateJavaScriptResultHelper.getId());
    }

    private void waitForCookie(final String url) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mCookieManager.getCookie(url) != null;
            }
        }));
    }

    private void validateCookies(String responseCookie, String... expectedCookieNames) {
        String[] cookies = responseCookie.split(";");
        Set<String> foundCookieNames = new HashSet<String>();
        for (String cookie : cookies) {
            foundCookieNames.add(cookie.substring(0, cookie.indexOf("=")).trim());
        }
        MoreAsserts.assertEquals(
                foundCookieNames, new HashSet<String>(Arrays.asList(expectedCookieNames)));
    }

    @MediumTest
    @Feature({"Android-WebView", "Privacy"})
    public void testRemoveAllCookie() throws InterruptedException {
        // enable cookie
        mCookieManager.setAcceptCookie(true);
        assertTrue(mCookieManager.acceptCookie());

        // first there should be no cookie stored
        mCookieManager.removeAllCookie();
        assertFalse(mCookieManager.hasCookies());

        String url = "http://www.example.com";
        String cookie = "name=test";
        mCookieManager.setCookie(url, cookie);
        assertEquals(cookie, mCookieManager.getCookie(url));

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mCookieManager.hasCookies();
            }
        }));

        // clean up all cookies
        mCookieManager.removeAllCookie();
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !mCookieManager.hasCookies();
            }
        }));
    }

    @MediumTest
    @Feature({"Android-WebView", "Privacy"})
    @SuppressWarnings("deprecation")
    public void testCookieExpiration() throws InterruptedException {
        // enable cookie
        mCookieManager.setAcceptCookie(true);
        assertTrue(mCookieManager.acceptCookie());
        mCookieManager.removeAllCookie();
        assertFalse(mCookieManager.hasCookies());

        final String url = "http://www.example.com";
        final String cookie1 = "cookie1=peter";
        final String cookie2 = "cookie2=sue";
        final String cookie3 = "cookie3=marc";

        mCookieManager.setCookie(url, cookie1); // session cookie

        Date date = new Date();
        date.setTime(date.getTime() + 1000 * 600);
        String value2 = cookie2 + "; expires=" + date.toGMTString();
        mCookieManager.setCookie(url, value2); // expires in 10min

        long expiration = 3000;
        date = new Date();
        date.setTime(date.getTime() + expiration);
        String value3 = cookie3 + "; expires=" + date.toGMTString();
        mCookieManager.setCookie(url, value3); // expires in 3s

        String allCookies = mCookieManager.getCookie(url);
        assertTrue(allCookies.contains(cookie1));
        assertTrue(allCookies.contains(cookie2));
        assertTrue(allCookies.contains(cookie3));

        mCookieManager.removeSessionCookie();
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                String c = mCookieManager.getCookie(url);
                return !c.contains(cookie1) && c.contains(cookie2) && c.contains(cookie3);
            }
        }));

        Thread.sleep(expiration + 1000); // wait for cookie to expire
        mCookieManager.removeExpiredCookie();
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                String c = mCookieManager.getCookie(url);
                return !c.contains(cookie1) && c.contains(cookie2) && !c.contains(cookie3);
            }
        }));

        mCookieManager.removeAllCookie();
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mCookieManager.getCookie(url) == null;
            }
        }));
    }
}
