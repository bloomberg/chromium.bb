// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.chrome.test.util.browser.WebappTestPage;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.HashMap;

/**
 * Tests the WebApkUpdateDataFetcher.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
public class WebApkUpdateDataFetcherTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String WEB_MANIFEST_URL1 = "/chrome/test/data/banners/manifest.json";
    // Name for Web Manifest at {@link WEB_MANIFEST_URL1}.
    private static final String WEB_MANIFEST_NAME1 = "Manifest test app";

    private static final String WEB_MANIFEST_URL2 =
            "/chrome/test/data/banners/manifest_short_name_only.json";
    // Name for Web Manifest at {@link WEB_MANIFEST_URL2}.
    private static final String WEB_MANIFEST_NAME2 = "Manifest";

    // Web Manifest with Murmur2 icon hash with value > {@link Long#MAX_VALUE}
    private static final String WEB_MANIFEST_WITH_LONG_ICON_MURMUR2_HASH =
            "/chrome/test/data/banners/manifest_long_icon_murmur2_hash.json";
    // Murmur2 hash of icon at {@link WEB_MANIFEST_WITH_LONG_ICON_MURMUR2_HASH}.
    private static final String LONG_ICON_MURMUR2_HASH = "13495109619211221667";

    // Scope for {@link WEB_MANIFEST_URL1}, {@link WEB_MANIFEST_URL2} and
    // {@link WEB_MANIFEST_WITH_LONG_ICON_MURMUR2_HASH}.
    private static final String WEB_MANIFEST_SCOPE = "/chrome/test/data";

    private EmbeddedTestServer mTestServer;
    private Tab mTab;

    // CallbackHelper which blocks until the {@link ManifestUpgradeDetectorFetcher.Callback}
    // callback is called.
    private static class CallbackWaiter
            extends CallbackHelper implements WebApkUpdateDataFetcher.Observer {
        private boolean mWebApkCompatible;
        private String mName;
        private String mBestIconMurmur2Hash;

        @Override
        public void onWebManifestForInitialUrlNotWebApkCompatible() {
            mWebApkCompatible = false;
            notifyCalled();
        }

        @Override
        public void onGotManifestData(WebApkInfo fetchedInfo, String bestIconUrl) {
            Assert.assertNull(mName);
            mWebApkCompatible = true;
            mName = fetchedInfo.name();
            mBestIconMurmur2Hash = fetchedInfo.iconUrlToMurmur2HashMap().get(bestIconUrl);
            notifyCalled();
        }

        public boolean isWebApkCompatible() {
            return mWebApkCompatible;
        }

        public String name() {
            return mName;
        }

        public String bestIconMurmur2Hash() {
            return mBestIconMurmur2Hash;
        }
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(context);
        mTab = mActivityTestRule.getActivity().getActivityTab();
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    /** Creates and starts a WebApkUpdateDataFetcher. */
    private void startWebApkUpdateDataFetcher(final String scopeUrl,
            final String manifestUrl, final WebApkUpdateDataFetcher.Observer observer) {
        final WebApkUpdateDataFetcher fetcher = new WebApkUpdateDataFetcher();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebApkInfo oldInfo = WebApkInfo.create("", "", false /* forceNavigation */,
                        scopeUrl, null, null, null, -1, -1, -1, -1, -1, "random.package", -1,
                        manifestUrl, "", new HashMap<String, String>());
                fetcher.start(mTab, oldInfo, observer);
            }
        });
    }

    /**
     * Test starting WebApkUpdateDataFetcher while a page with the desired manifest URL is loading.
     */
    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testLaunchWithDesiredManifestUrl() throws Exception {
        WebappTestPage.navigateToPageWithServiceWorkerAndManifest(
                mTestServer, mTab, WEB_MANIFEST_URL1);

        CallbackWaiter waiter = new CallbackWaiter();
        startWebApkUpdateDataFetcher(mTestServer.getURL(WEB_MANIFEST_SCOPE),
                mTestServer.getURL(WEB_MANIFEST_URL1), waiter);
        waiter.waitForCallback(0);

        Assert.assertTrue(waiter.isWebApkCompatible());
        Assert.assertEquals(WEB_MANIFEST_NAME1, waiter.name());
    }

    /**
     * Test starting WebApkUpdateDataFetcher on page which uses a different manifest URL than the
     * ManifestUpgradeDetectorFetcher is looking for. Check that the callback is only called once
     * the user navigates to a page which uses the desired manifest URL.
     */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testLaunchWithDifferentManifestUrl() throws Exception {
        WebappTestPage.navigateToPageWithServiceWorkerAndManifest(
                mTestServer, mTab, WEB_MANIFEST_URL1);

        CallbackWaiter waiter = new CallbackWaiter();
        startWebApkUpdateDataFetcher(mTestServer.getURL(WEB_MANIFEST_SCOPE),
                mTestServer.getURL(WEB_MANIFEST_URL2), waiter);
        waiter.waitForCallback(0);
        Assert.assertFalse(waiter.isWebApkCompatible());

        WebappTestPage.navigateToPageWithServiceWorkerAndManifest(
                mTestServer, mTab, WEB_MANIFEST_URL2);
        waiter.waitForCallback(1);
        Assert.assertTrue(waiter.isWebApkCompatible());
        Assert.assertEquals(WEB_MANIFEST_NAME2, waiter.name());
    }

    /**
     * Test that {@link onWebManifestForInitialUrlNotWebApkCompatible()} is called after attempting
     * to fetch Web Manifest for page with no Web Manifest.
     */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testNoWebManifest() throws Exception {
        new TabLoadObserver(mTab).fullyLoadUrl(
                mTestServer.getURL("/chrome/test/data/banners/no_manifest_test_page.html"));

        CallbackWaiter waiter = new CallbackWaiter();
        startWebApkUpdateDataFetcher(mTestServer.getURL(WEB_MANIFEST_SCOPE),
                mTestServer.getURL(WEB_MANIFEST_URL2), waiter);
        waiter.waitForCallback(0);
        Assert.assertFalse(waiter.isWebApkCompatible());
    }

    /**
     * Test that large icon murmur2 hashes are correctly plumbed to Java. The hash can take on
     * values up to 2^64 - 1 which is greater than {@link Long#MAX_VALUE}.
     */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testLargeIconMurmur2Hash() throws Exception {
        WebappTestPage.navigateToPageWithServiceWorkerAndManifest(
                mTestServer, mTab, WEB_MANIFEST_WITH_LONG_ICON_MURMUR2_HASH);

        CallbackWaiter waiter = new CallbackWaiter();
        startWebApkUpdateDataFetcher(mTestServer.getURL(WEB_MANIFEST_SCOPE),
                mTestServer.getURL(WEB_MANIFEST_WITH_LONG_ICON_MURMUR2_HASH), waiter);
        waiter.waitForCallback(0);

        Assert.assertEquals(LONG_ICON_MURMUR2_HASH, waiter.bestIconMurmur2Hash());
    }
}
