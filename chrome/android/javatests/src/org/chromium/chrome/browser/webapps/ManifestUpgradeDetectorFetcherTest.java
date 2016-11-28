// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.browser.WebappTestPage;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.HashMap;

/**
 * Tests the ManifestUpgradeDetectorFetcher.
 */
public class ManifestUpgradeDetectorFetcherTest extends ChromeTabbedActivityTestBase {

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
            extends CallbackHelper implements ManifestUpgradeDetectorFetcher.Callback {
        private String mName;
        private String mBestIconMurmur2Hash;

        @Override
        public void onGotManifestData(WebApkInfo fetchedInfo, String bestIconUrl) {
            assertNull(mName);
            mName = fetchedInfo.name();
            mBestIconMurmur2Hash = fetchedInfo.iconUrlToMurmur2HashMap().get(bestIconUrl);
            notifyCalled();
        }

        public String name() {
            return mName;
        }

        public String bestIconMurmur2Hash() {
            return mBestIconMurmur2Hash;
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        Context context = getInstrumentation().getTargetContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(context);
        mTab = getActivity().getActivityTab();
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    /**
     * Starts a ManifestUpgradeDetectorFetcher. Calls {@link callback} once the fetcher is done.
     */
    private void startManifestUpgradeDetectorFetcher(final String scopeUrl,
            final String manifestUrl, final ManifestUpgradeDetectorFetcher.Callback callback) {
        final ManifestUpgradeDetectorFetcher fetcher = new ManifestUpgradeDetectorFetcher();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                WebApkInfo oldInfo = WebApkInfo.create("", "", scopeUrl, null, null, null, -1, -1,
                        -1, -1, -1, "random.package", -1, manifestUrl, null,
                        new HashMap<String, String>());
                fetcher.start(mTab, oldInfo, callback);
            }
        });
    }

    /**
     * Test starting ManifestUpgradeDetectorFetcher while a page with the desired manifest URL is
     * loading.
     */
    @MediumTest
    @Feature({"WebApk"})
    public void testLaunchWithDesiredManifestUrl() throws Exception {
        CallbackWaiter waiter = new CallbackWaiter();
        startManifestUpgradeDetectorFetcher(mTestServer.getURL(WEB_MANIFEST_SCOPE),
                mTestServer.getURL(WEB_MANIFEST_URL1), waiter);

        WebappTestPage.navigateToPageWithServiceWorkerAndManifest(
                mTestServer, mTab, WEB_MANIFEST_URL1);
        waiter.waitForCallback(0);

        assertEquals(WEB_MANIFEST_NAME1, waiter.name());
    }

    /**
     * Test starting ManifestUpgradeDetectorFetcher on page which uses a different manifest URL than
     * the ManifestUpgradeDetectorFetcher is looking for. Check that the callback is only called
     * once the user navigates to a page which uses the desired manifest URL.
     */
    @MediumTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testLaunchWithDifferentManifestUrl() throws Exception {
        CallbackWaiter waiter = new CallbackWaiter();
        startManifestUpgradeDetectorFetcher(mTestServer.getURL(WEB_MANIFEST_SCOPE),
                mTestServer.getURL(WEB_MANIFEST_URL2), waiter);

        WebappTestPage.navigateToPageWithServiceWorkerAndManifest(
                mTestServer, mTab, WEB_MANIFEST_URL1);
        WebappTestPage.navigateToPageWithServiceWorkerAndManifest(
                mTestServer, mTab, WEB_MANIFEST_URL2);
        waiter.waitForCallback(0);

        assertEquals(WEB_MANIFEST_NAME2, waiter.name());
    }

    /**
     * Test that large icon murmur2 hashes are correctly plumbed to Java. The hash can take on
     * values up to 2^64 - 1 which is greater than {@link Long#MAX_VALUE}.
     */
    @MediumTest
    @Feature({"Webapps"})
    public void testLargeIconMurmur2Hash() throws Exception {
        CallbackWaiter waiter = new CallbackWaiter();
        startManifestUpgradeDetectorFetcher(mTestServer.getURL(WEB_MANIFEST_SCOPE),
                mTestServer.getURL(WEB_MANIFEST_WITH_LONG_ICON_MURMUR2_HASH), waiter);
        WebappTestPage.navigateToPageWithServiceWorkerAndManifest(
                mTestServer, mTab, WEB_MANIFEST_WITH_LONG_ICON_MURMUR2_HASH);
        waiter.waitForCallback(0);

        assertEquals(LONG_ICON_MURMUR2_HASH, waiter.bestIconMurmur2Hash());
    }
}
