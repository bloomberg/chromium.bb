// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;
import android.util.Pair;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.android_webview.AwContents;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for the AwContentsClient.onReceivedLoginRequest callback.
 */
public class AwContentsClientAutoLoginTest extends AndroidWebViewTestBase {
    public static class OnReceivedLoginRequestHelper extends CallbackHelper {
        String mRealm;
        String mAccount;
        String mArgs;

        public String getRealm() {
            assert getCallCount() > 0;
            return mRealm;
        }

        public String getAccount() {
            assert getCallCount() > 0;
            return mAccount;
        }

        public String getArgs() {
            assert getCallCount() > 0;
            return mArgs;
        }

        public void notifyCalled(String realm, String account, String args) {
            mRealm = realm;
            mAccount = account;
            mArgs = args;
            notifyCalled();
        }
    }

    private static class TestAwContentsClient
            extends org.chromium.android_webview.test.TestAwContentsClient {

        private OnReceivedLoginRequestHelper mOnReceivedLoginRequestHelper;

        public TestAwContentsClient() {
            mOnReceivedLoginRequestHelper = new OnReceivedLoginRequestHelper();
        }

        public OnReceivedLoginRequestHelper getOnReceivedLoginRequestHelper() {
            return mOnReceivedLoginRequestHelper;
        }

        @Override
        public void onReceivedLoginRequest(String realm, String account, String args) {
            getOnReceivedLoginRequestHelper().notifyCalled(realm, account, args);
        }
    }

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();

    private void autoLoginTestHelper(final String testName, final String xAutoLoginHeader,
            final String expectedRealm, final String expectedAccount, final String expectedArgs)
            throws Throwable {
        AwTestContainerView testView = createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();
        final OnReceivedLoginRequestHelper loginRequestHelper =
            mContentsClient.getOnReceivedLoginRequestHelper();

        final String path = "/" + testName + ".html";
        final String html = testName;
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("x-auto-login", xAutoLoginHeader));

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String pageUrl = webServer.setResponse(path, html, headers);
            final int callCount = loginRequestHelper.getCallCount();
            loadUrlAsync(awContents, pageUrl);
            loginRequestHelper.waitForCallback(callCount);

            assertEquals(expectedRealm, loginRequestHelper.getRealm());
            assertEquals(expectedAccount, loginRequestHelper.getAccount());
            assertEquals(expectedArgs, loginRequestHelper.getArgs());
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testAutoLoginOnGoogleCom() throws Throwable {
        autoLoginTestHelper(
                "testAutoLoginOnGoogleCom",  /* testName */
                "realm=com.google&account=foo%40bar.com&args=random_string", /* xAutoLoginHeader */
                "com.google",  /* expectedRealm */
                "foo@bar.com",  /* expectedAccount */
                "random_string"  /* expectedArgs */);

    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testAutoLoginWithNullAccount() throws Throwable {
        autoLoginTestHelper(
                "testAutoLoginOnGoogleCom",  /* testName */
                "realm=com.google&args=not.very.inventive", /* xAutoLoginHeader */
                "com.google",  /* expectedRealm */
                null,  /* expectedAccount */
                "not.very.inventive"  /* expectedArgs */);
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testAutoLoginOnNonGoogle() throws Throwable {
        autoLoginTestHelper(
                "testAutoLoginOnGoogleCom",  /* testName */
                "realm=com.bar&account=foo%40bar.com&args=args", /* xAutoLoginHeader */
                "com.bar",  /* expectedRealm */
                "foo@bar.com",  /* expectedAccount */
                "args"  /* expectedArgs */);
    }
}
