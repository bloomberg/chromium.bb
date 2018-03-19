// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.support.customtabs.CustomTabsService;
import android.support.customtabs.CustomTabsSessionToken;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.blink_public.web.WebReferrerPolicy;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.OriginVerifier;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;

/** Tests for detached resource requests. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class DetachedResourceRequestTest {
    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private CustomTabsConnection mConnection;
    private Context mContext;
    private EmbeddedTestServer mServer;

    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chrome";
    private static final Uri ORIGIN = Uri.parse("http://cats.google.com");

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized();
        mConnection = CustomTabsTestUtils.setUpConnection();
        mContext = InstrumentationRegistry.getInstrumentation()
                           .getTargetContext()
                           .getApplicationContext();
    }

    @After
    public void tearDown() throws Exception {
        CustomTabsTestUtils.cleanupSessions(mConnection);
        if (mServer != null) mServer.stopAndDestroyServer();
        mServer = null;
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_PARALLEL_REQUEST)
    public void testCanDoParallelRequest() throws Exception {
        CustomTabsSessionToken session = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mConnection.newSession(session));
        ThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertFalse(mConnection.canDoParallelRequest(session, ORIGIN)); });
        CustomTabsTestUtils.warmUpAndWait();
        ThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertFalse(mConnection.canDoParallelRequest(session, ORIGIN)); });
        ThreadUtils.runOnUiThreadBlocking(() -> {
            String packageName = mContext.getPackageName();
            OriginVerifier.addVerifiedOriginForPackage(packageName, new Origin(ORIGIN.toString()),
                    CustomTabsService.RELATION_USE_AS_ORIGIN);
            Assert.assertTrue(mConnection.canDoParallelRequest(session, ORIGIN));
        });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_PARALLEL_REQUEST)
    public void testStartParallelRequestValidation() throws Exception {
        CustomTabsSessionToken session = prepareSession();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse("Should not allow android-app:// scheme",
                    mConnection.startParallelRequest(session,
                            Uri.parse("android-app://this.is.an.android.app"), ORIGIN,
                            WebReferrerPolicy.DEFAULT));
            Assert.assertFalse("Should not allow an empty URL",
                    mConnection.startParallelRequest(
                            session, Uri.parse(""), ORIGIN, WebReferrerPolicy.DEFAULT));
            Assert.assertFalse("Should not allow an arbitrary origin",
                    mConnection.startParallelRequest(session, Uri.parse("HTTPS://foo.bar"),
                            Uri.parse("wrong://origin"), WebReferrerPolicy.DEFAULT));
            Assert.assertTrue(mConnection.startParallelRequest(
                    session, Uri.parse("HTTP://foo.bar"), ORIGIN, WebReferrerPolicy.DEFAULT));
        });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_PARALLEL_REQUEST)
    public void testCanStartParallelRequest() throws Exception {
        CustomTabsSessionToken session = prepareSession();
        mServer = new EmbeddedTestServer();
        final CallbackHelper cb = new CallbackHelper();
        mServer.initializeNative(mContext, EmbeddedTestServer.ServerHTTPSSetting.USE_HTTP);
        mServer.setConnectionListener(new EmbeddedTestServer.ConnectionListener() {
            @Override
            public void readFromSocket(long socketId) {
                cb.notifyCalled();
            }
        });
        mServer.start();

        Uri url = Uri.parse(mServer.getURL("/echotitle"));
        ThreadUtils.runOnUiThread(() -> {
            Assert.assertTrue(mConnection.startParallelRequest(
                    session, url, ORIGIN, WebReferrerPolicy.DEFAULT));
        });
        cb.waitForCallback(0, 1);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_PARALLEL_REQUEST)
    public void testCanSetCookie() throws Exception {
        CustomTabsSessionToken session = prepareSession();
        mServer = EmbeddedTestServer.createAndStartServer(mContext);
        final Uri url = Uri.parse(mServer.getURL("/set-cookie?acookie"));
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(mConnection.startParallelRequest(
                    session, url, ORIGIN, WebReferrerPolicy.DEFAULT));
        });

        String echoUrl = mServer.getURL("/echoheader?Cookie");
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, echoUrl);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String content = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "document.body.textContent");
        Assert.assertEquals("\"acookie\"", content);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_PARALLEL_REQUEST)
    public void testCanBlockThirdPartyCookies() throws Exception {
        CustomTabsSessionToken session = prepareSession();
        mServer = EmbeddedTestServer.createAndStartServer(mContext);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            PrefServiceBridge prefs = PrefServiceBridge.getInstance();
            Assert.assertFalse(prefs.isBlockThirdPartyCookiesEnabled());
            prefs.setBlockThirdPartyCookiesEnabled(true);
        });
        final Uri url = Uri.parse(mServer.getURL("/set-cookie?acookie"));
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(mConnection.startParallelRequest(
                    session, url, ORIGIN, WebReferrerPolicy.DEFAULT));
        });

        String echoUrl = mServer.getURL("/echoheader?Cookie");
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, echoUrl);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String content = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "document.body.textContent");
        Assert.assertEquals("\"None\"", content);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_PARALLEL_REQUEST)
    public void testThirdPartyCookieBlockingAllowsFirstParty() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        mServer = EmbeddedTestServer.createAndStartServer(mContext);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            PrefServiceBridge prefs = PrefServiceBridge.getInstance();
            Assert.assertFalse(prefs.isBlockThirdPartyCookiesEnabled());
            prefs.setBlockThirdPartyCookiesEnabled(true);
        });
        final Uri url = Uri.parse(mServer.getURL("/set-cookie?acookie"));
        final Uri origin = Uri.parse(new Origin(url).toString());
        CustomTabsSessionToken session = prepareSession(url);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(mConnection.startParallelRequest(
                    session, url, origin, WebReferrerPolicy.DEFAULT));
        });

        String echoUrl = mServer.getURL("/echoheader?Cookie");
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, echoUrl);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String content = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "document.body.textContent");
        Assert.assertEquals("\"acookie\"", content);
    }

    private CustomTabsSessionToken prepareSession() throws Exception {
        return prepareSession(ORIGIN);
    }

    private CustomTabsSessionToken prepareSession(Uri origin) throws Exception {
        final CustomTabsSessionToken session =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mConnection.newSession(session));
        CustomTabsTestUtils.warmUpAndWait();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            OriginVerifier.addVerifiedOriginForPackage(mContext.getPackageName(),
                    new Origin(origin.toString()), CustomTabsService.RELATION_USE_AS_ORIGIN);
            Assert.assertTrue(mConnection.canDoParallelRequest(session, origin));
        });
        return session;
    }
}
