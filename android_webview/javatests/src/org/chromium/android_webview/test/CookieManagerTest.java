// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.MoreAsserts;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Pair;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Callable;

/**
 * Tests for the CookieManager.
 */
public class CookieManagerTest extends AwTestBase {

    private AwCookieManager mCookieManager;
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mCookieManager = new AwCookieManager();
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mAwContents.getSettings().setJavaScriptEnabled(true);
        assertNotNull(mCookieManager);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testAllowFileSchemeCookies() throws Throwable {
        assertFalse(mCookieManager.allowFileSchemeCookies());
        mCookieManager.setAcceptFileSchemeCookies(true);
        assertTrue(mCookieManager.allowFileSchemeCookies());
        mCookieManager.setAcceptFileSchemeCookies(false);
        assertFalse(mCookieManager.allowFileSchemeCookies());
    }

    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
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

            loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            setCookie("test1", "value1");
            assertNull(mCookieManager.getCookie(url));

            List<Pair<String, String>> responseHeaders = new ArrayList<Pair<String, String>>();
            responseHeaders.add(
                    Pair.create("Set-Cookie", "header-test1=header-value1; path=" + path));
            url = webServer.setResponse(path, responseStr, responseHeaders);
            loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            assertNull(mCookieManager.getCookie(url));

            mCookieManager.setAcceptCookie(true);
            assertTrue(mCookieManager.acceptCookie());

            url = webServer.setResponse(path, responseStr, null);
            loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            setCookie("test2", "value2");
            waitForCookie(url);
            String cookie = mCookieManager.getCookie(url);
            assertNotNull(cookie);
            validateCookies(cookie, "test2");

            responseHeaders = new ArrayList<Pair<String, String>>();
            responseHeaders.add(
                    Pair.create("Set-Cookie", "header-test2=header-value2 path=" + path));
            url = webServer.setResponse(path, responseStr, responseHeaders);
            loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
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
        JSUtils.executeJavaScriptAndWaitForResult(
                this, mAwContents,
                mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                "var expirationDate = new Date();" +
                "expirationDate.setDate(expirationDate.getDate() + 5);" +
                "document.cookie='" + name + "=" + value +
                        "; expires=' + expirationDate.toUTCString();");
    }

    private void waitForCookie(final String url) throws Exception {
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mCookieManager.getCookie(url) != null;
            }
        });
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
    @Feature({"AndroidWebView", "Privacy"})
    public void testRemoveAllCookie() throws Exception {
        // enable cookie
        mCookieManager.setAcceptCookie(true);
        assertTrue(mCookieManager.acceptCookie());

        // first there should be no cookie stored
        mCookieManager.removeAllCookie();
        mCookieManager.flushCookieStore();
        assertFalse(mCookieManager.hasCookies());

        String url = "http://www.example.com";
        String cookie = "name=test";
        mCookieManager.setCookie(url, cookie);
        assertEquals(cookie, mCookieManager.getCookie(url));

        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mCookieManager.hasCookies();
            }
        });

        // clean up all cookies
        mCookieManager.removeAllCookie();
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return !mCookieManager.hasCookies();
            }
        });
    }

    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    @SuppressWarnings("deprecation")
    public void testCookieExpiration() throws Exception {
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
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                String c = mCookieManager.getCookie(url);
                return !c.contains(cookie1) && c.contains(cookie2) && c.contains(cookie3);
            }
        });

        Thread.sleep(expiration + 1000); // wait for cookie to expire
        mCookieManager.removeExpiredCookie();
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                String c = mCookieManager.getCookie(url);
                return !c.contains(cookie1) && c.contains(cookie2) && !c.contains(cookie3);
            }
        });

        mCookieManager.removeAllCookie();
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mCookieManager.getCookie(url) == null;
            }
        });
    }

    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testThirdPartyCookie() throws Throwable {
        TestWebServer webServer = null;
        try {
            // In theory we need two servers to test this, one server ('the first party')
            // which returns a response with a link to a second server ('the third party')
            // at different origin. This second server attempts to set a cookie which should
            // fail if AcceptThirdPartyCookie() is false.
            // Strictly according to the letter of RFC6454 it should be possible to set this
            // situation up with two TestServers on different ports (these count as having
            // different origins) but Chrome is not strict about this and does not check the
            // port. Instead we cheat making some of the urls come from localhost and some
            // from 127.0.0.1 which count (both in theory and pratice) as having different
            // origins.
            webServer = new TestWebServer(false);

            // Turn global allow on.
            mCookieManager.setAcceptCookie(true);
            mCookieManager.removeAllCookie();
            assertTrue(mCookieManager.acceptCookie());
            assertFalse(mCookieManager.hasCookies());

            // When third party cookies are disabled...
            mCookieManager.setAcceptThirdPartyCookie(false);
            assertFalse(mCookieManager.acceptThirdPartyCookie());

            // ...we can't set third party cookies.
            // First on the third party server we create a url which tries to set a cookie.
            String cookieUrl = toThirdPartyUrl(
                    makeCookieUrl(webServer, "/cookie_1.js", "test1", "value1"));
            // Then we create a url on the first party server which links to the first url.
            String url = makeScriptLinkUrl(webServer, "/content_1.html", cookieUrl);
            loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            assertNull(mCookieManager.getCookie(cookieUrl));

            // When third party cookies are enabled...
            mCookieManager.setAcceptThirdPartyCookie(true);
            assertTrue(mCookieManager.acceptThirdPartyCookie());

            // ...we can set third party cookies.
            cookieUrl = toThirdPartyUrl(
                    makeCookieUrl(webServer, "/cookie_2.js", "test2", "value2"));
            url = makeScriptLinkUrl(webServer, "/content_2.html", cookieUrl);
            loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            waitForCookie(cookieUrl);
            String cookie = mCookieManager.getCookie(cookieUrl);
            assertNotNull(cookie);
            validateCookies(cookie, "test2");
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    /**
     * Creates a response on the TestWebServer which attempts to set a cookie when fetched.
     * @param  webServer  the webServer on which to create the response
     * @param  path the path component of the url (e.g "/cookie_test.html")
     * @param  key the key of the cookie
     * @param  value the value of the cookie
     * @return  the url which gets the response
     */
    private String makeCookieUrl(TestWebServer webServer, String path, String key, String value) {
        String response = "";
        List<Pair<String, String>> responseHeaders = new ArrayList<Pair<String, String>>();
        responseHeaders.add(
            Pair.create("Set-Cookie", key + "=" + value + "; path=" + path));
        return webServer.setResponse(path, response, responseHeaders);
    }

    /**
     * Creates a response on the TestWebServer which contains a script tag with an external src.
     * @param  webServer  the webServer on which to create the response
     * @param  path the path component of the url (e.g "/my_thing_with_script.html")
     * @param  url the url which which should appear as the src of the script tag.
     * @return  the url which gets the response
     */
    private String makeScriptLinkUrl(TestWebServer webServer, String path, String url) {
        String responseStr = "<html><head><title>Content!</title></head>" +
                    "<body><script src=" + url + "></script></body></html>";
        return webServer.setResponse(path, responseStr, null);
    }

    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testThirdPartyJavascriptCookie() throws Throwable {
        TestWebServer webServer = null;
        try {
            // This test again uses 127.0.0.1/localhost trick to simulate a third party.
            webServer = new TestWebServer(false);

            mCookieManager.setAcceptCookie(true);
            mCookieManager.removeAllCookie();
            assertTrue(mCookieManager.acceptCookie());
            assertFalse(mCookieManager.hasCookies());

            // When third party cookies are disabled...
            mCookieManager.setAcceptThirdPartyCookie(false);
            assertFalse(mCookieManager.acceptThirdPartyCookie());

            // ...we can't set third party cookies.
            // We create a script which tries to set a cookie on a third party.
            String cookieUrl = toThirdPartyUrl(
                    makeCookieScriptUrl(webServer, "/cookie_1.html", "test1", "value1"));
            // Then we load it as an iframe.
            String url = makeIframeUrl(webServer, "/content_1.html", cookieUrl);
            loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            assertNull(mCookieManager.getCookie(cookieUrl));

            // When third party cookies are enabled...
            mCookieManager.setAcceptThirdPartyCookie(true);
            assertTrue(mCookieManager.acceptThirdPartyCookie());

            // ...we can set third party cookies.
            cookieUrl = toThirdPartyUrl(
                    makeCookieScriptUrl(webServer, "/cookie_2.html", "test2", "value2"));
            url = makeIframeUrl(webServer, "/content_2.html", cookieUrl);
            loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            String cookie = mCookieManager.getCookie(cookieUrl);
            assertNotNull(cookie);
            validateCookies(cookie, "test2");
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    /**
     * Creates a response on the TestWebServer which attempts to set a cookie when fetched.
     * @param  webServer  the webServer on which to create the response
     * @param  path the path component of the url (e.g "/my_thing_with_iframe.html")
     * @param  url the url which which should appear as the src of the iframe.
     * @return  the url which gets the response
     */
    private String makeIframeUrl(TestWebServer webServer, String path, String url) {
        String responseStr = "<html><head><title>Content!</title></head>" +
                    "<body><iframe src=" + url + "></iframe></body></html>";
        return webServer.setResponse(path, responseStr, null);
    }

    /**
     * Creates a response on the TestWebServer with a script that attempts to set a cookie.
     * @param  webServer  the webServer on which to create the response
     * @param  path the path component of the url (e.g "/cookie_test.html")
     * @param  key the key of the cookie
     * @param  value the value of the cookie
     * @return  the url which gets the response
     */
    private String makeCookieScriptUrl(TestWebServer webServer, String path, String key,
            String value) {
        String response = "<html><head></head><body>" +
            "<script>document.cookie = \"" + key + "=" + value + "\";</script></body></html>";
        return webServer.setResponse(path, response, null);
    }

    /**
     * Makes a url look as if it comes from a different host.
     * @param  url the url to fake.
     * @return  the resulting after faking.
     */
    private String toThirdPartyUrl(String url) {
        return url.replace("localhost", "127.0.0.1");
    }
}
