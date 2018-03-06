// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
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
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.browserservices.OriginVerifier;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.net.test.EmbeddedTestServer;

/** Tests for detached resource requests. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class DetachedResourceRequestTest {
    @Rule
    public TestRule processor = new Features.InstrumentationProcessor();

    private CustomTabsConnection mConnection;
    private Context mContext;

    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chrome";
    private static final String ORIGIN = "http://cats.google.com";

    @Before
    public void setUp() throws Exception {
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
            OriginVerifier.addVerifiedOriginForPackage(
                    packageName, Uri.parse(ORIGIN), CustomTabsService.RELATION_USE_AS_ORIGIN);
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
                    mConnection.startParallelRequest(
                            session, "android-app://this.is.an.android.app", ORIGIN));
            Assert.assertFalse("Should not allow an empty URL",
                    mConnection.startParallelRequest(session, "", ORIGIN));
            Assert.assertFalse("Should not allow an arbitrary origin",
                    mConnection.startParallelRequest(session, "HTTPS://foo.bar", "wrong://origin"));
            Assert.assertTrue(mConnection.startParallelRequest(session, "HTTP://foo.bar", ORIGIN));
        });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_PARALLEL_REQUEST)
    public void testCanStartParallelRequest() throws Exception {
        CustomTabsSessionToken session = prepareSession();

        final CallbackHelper cb = new CallbackHelper();
        EmbeddedTestServer server = new EmbeddedTestServer();
        try {
            server.initializeNative(mContext, EmbeddedTestServer.ServerHTTPSSetting.USE_HTTP);
            server.setConnectionListener(new EmbeddedTestServer.ConnectionListener() {
                @Override
                public void readFromSocket(long socketId) {
                    cb.notifyCalled();
                }
            });
            server.start();

            ThreadUtils.runOnUiThread(() -> {
                String url = server.getURL("/echotitle");
                Assert.assertTrue(mConnection.startParallelRequest(session, url, ORIGIN));
            });
            cb.waitForCallback(0, 1);
        } finally {
            server.stopAndDestroyServer();
        }
    }

    private CustomTabsSessionToken prepareSession() throws Exception {
        final CustomTabsSessionToken session =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mConnection.newSession(session));
        CustomTabsTestUtils.warmUpAndWait();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            OriginVerifier.addVerifiedOriginForPackage(mContext.getPackageName(), Uri.parse(ORIGIN),
                    CustomTabsService.RELATION_USE_AS_ORIGIN);
            Assert.assertTrue(mConnection.canDoParallelRequest(session, ORIGIN));
        });
        return session;
    }
}
