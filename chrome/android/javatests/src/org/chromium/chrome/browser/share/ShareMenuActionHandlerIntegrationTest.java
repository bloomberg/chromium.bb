// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.share.ShareMenuActionHandler.ShareMenuActionDelegate;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration tests for the Share Menu handling.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class ShareMenuActionHandlerIntegrationTest {
    private static final String PAGE_WITH_HTTPS_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_https_canonical.html";
    private static final String PAGE_WITH_HTTP_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_http_canonical.html";
    private static final String PAGE_WITH_NO_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_no_canonical.html";

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @SmallTest
    public void testCanonicalUrlsOverHttps() throws InterruptedException, TimeoutException {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        final String httpsCanonicalUrl = testServer.getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        final String httpCanonicalUrl = testServer.getURL(PAGE_WITH_HTTP_CANONICAL_URL);
        final String noCanonicalUrl = testServer.getURL(PAGE_WITH_NO_CANONICAL_URL);

        try {
            verifyShareUrl(httpsCanonicalUrl, "https://examplehttps.com/");
            verifyShareUrl(httpCanonicalUrl, httpCanonicalUrl);
            verifyShareUrl(noCanonicalUrl, noCanonicalUrl);
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    @Test
    @SmallTest
    public void testCanonicalUrlsOverHttp() throws InterruptedException, TimeoutException {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        final String httpsCanonicalUrl = testServer.getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        final String httpCanonicalUrl = testServer.getURL(PAGE_WITH_HTTP_CANONICAL_URL);
        final String noCanonicalUrl = testServer.getURL(PAGE_WITH_NO_CANONICAL_URL);

        try {
            verifyShareUrl(httpsCanonicalUrl, httpsCanonicalUrl);
            verifyShareUrl(httpCanonicalUrl, httpCanonicalUrl);
            verifyShareUrl(noCanonicalUrl, noCanonicalUrl);
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    private void verifyShareUrl(String pageUrl, String expectedShareUrl)
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        mActivityTestRule.loadUrl(pageUrl);
        ShareParams params = triggerShare();
        Assert.assertEquals(expectedShareUrl, params.getUrl());
    }

    private ShareParams triggerShare() throws InterruptedException, TimeoutException {
        final CallbackHelper helper = new CallbackHelper();
        final AtomicReference<ShareParams> paramsRef = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ShareMenuActionDelegate delegate = new ShareMenuActionDelegate() {
                @Override
                void share(ShareParams params) {
                    paramsRef.set(params);
                    helper.notifyCalled();
                }
            };

            new ShareMenuActionHandler(delegate).onShareMenuItemSelected(
                    mActivityTestRule.getActivity(),
                    mActivityTestRule.getActivity().getActivityTab(), false, false);
        });
        helper.waitForCallback(0);
        return paramsRef.get();
    }
}
