// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.graphics.Bitmap;
import android.os.Environment;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests the ManifestUpgradeDetectorFetcher.
 */
public class ManifestUpgradeDetectorFetcherTest extends ChromeTabbedActivityTestBase {

    private static final String PAGE_URL1 = "/chrome/test/data/banners/manifest_test_page.html";
    private static final String PAGE_URL2 =
            "/chrome/test/data/banners/manifest_short_name_only_test_page.html";

    // Data for {@link PAGE_URL1}'s Web Manifest.
    private static final String WEB_MANIFEST_URL1 = "/chrome/test/data/banners/manifest.json";
    private static final String WEB_MANIFEST_NAME1 = "Manifest test app";

    // Data for {@link PAGE_URL2}'s Web Manifest.
    private static final String WEB_MANIFEST_URL2 =
            "/chrome/test/data/banners/manifest_short_name_only.json";
    private static final String WEB_MANIFEST_NAME2 = "Manifest";

    // Scope for {@link PAGE_URL1} and {@link PAGE_URL2}.
    private static final String WEB_MANIFEST_SCOPE = "/chrome/test/data";

    private EmbeddedTestServer mTestServer;
    private Tab mTab;

    // CallbackHelper which blocks until the {@link ManifestUpgradeDetectorFetcher.Callback}
    // callback is called.
    private static class CallbackWaiter
            extends CallbackHelper implements ManifestUpgradeDetectorFetcher.Callback {
        private String mName;

        @Override
        public void onGotManifestData(String startUrl, String scopeUrl, String name,
                String shortName, String iconUrl, long iconMurmur2Hash, Bitmap iconBitmap,
                int displayMode, int orientation, long themeColor, long backgroundColor) {
            assertNull(mName);
            mName = name;
            notifyCalled();
        }

        public String name() {
            return mName;
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        Context context = getInstrumentation().getTargetContext();
        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                context, Environment.getExternalStorageDirectory());
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
    private void startManifestUpgradeDetectorFetcher(String scopeUrl, String manifestUrl,
            final ManifestUpgradeDetectorFetcher.Callback callback) {
        final ManifestUpgradeDetectorFetcher fetcher =
                new ManifestUpgradeDetectorFetcher(mTab, scopeUrl, manifestUrl);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                fetcher.start(callback);
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

        TabLoadObserver tabLoadObserver = new TabLoadObserver(mTab);
        tabLoadObserver.fullyLoadUrl(mTestServer.getURL(PAGE_URL1));
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
    public void testLaunchWithDifferentManifestUrl() throws Exception {
        CallbackWaiter waiter = new CallbackWaiter();
        startManifestUpgradeDetectorFetcher(mTestServer.getURL(WEB_MANIFEST_SCOPE),
                mTestServer.getURL(WEB_MANIFEST_URL2), waiter);

        TabLoadObserver tabLoadObserver = new TabLoadObserver(mTab);
        tabLoadObserver.fullyLoadUrl(mTestServer.getURL(PAGE_URL1));
        tabLoadObserver.fullyLoadUrl(mTestServer.getURL(PAGE_URL2));
        waiter.waitForCallback(0);

        assertEquals(WEB_MANIFEST_NAME2, waiter.name());
    }
}
