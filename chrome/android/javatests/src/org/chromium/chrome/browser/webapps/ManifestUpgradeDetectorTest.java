// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.os.Environment;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.webapk.lib.common.WebApkConstants;

/**
 * Tests the ManifestUpgradeDetector.
 */
public class ManifestUpgradeDetectorTest extends ChromeTabbedActivityTestBase {
    private static final String WEBAPK_ID = WebApkConstants.WEBAPK_ID_PREFIX + "webapp_id";

    /** The following field values are declared in {@link WEBAPK_WEB_MANIFEST_URL}. */
    private static final String WEBAPK_NAME = "Manifest test app";
    private static final String WEBAPK_SHORT_NAME = "App";
    private static final int WEBAPK_ORIENTATION = ScreenOrientationValues.LANDSCAPE;
    private static final String WEBAPK_START_URL_PATH =
            "/chrome/test/data/webapps/manifest_test_page.html";
    private static final String WEBAPK_SCOPE_PATH = "/chrome/test/data/webapps/";
    private static final String WEBAPK_WEB_MANIFEST_URL =
            "/chrome/test/data/webapps/manifest.json";

    private TestManifestUpgradeDetector mDetector;
    private EmbeddedTestServer mTestServer;

    /**
     * A ManifestUpgradeDetector that verifies the Web Manifest fetching pipeline and the
     * comparison are working correctly.
     */
    private static class TestManifestUpgradeDetector extends ManifestUpgradeDetector {
        public boolean mIsUpgraded;
        public String mStartUrl;
        public CallbackHelper mCallbackHelper;

        public TestManifestUpgradeDetector(Tab tab, WebappInfo info) {
            super(tab, info);
            mCallbackHelper = new CallbackHelper();
        }

        @Override
        protected boolean requireUpgrade(WebappInfo newInfo) {
            mIsUpgraded = super.requireUpgrade(newInfo);
            mStartUrl = newInfo.uri().toString();
            mCallbackHelper.notifyCalled();
            return mIsUpgraded;
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        final Context context = getInstrumentation().getTargetContext();
        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                context, Environment.getExternalStorageDirectory());
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
     * Creates a WebappInfo with the properties in {@link WEBAPK_WEB_MANIFEST_URL}.
     */
    private WebappInfo createWebappInfo() {
        return WebappInfo.create(WEBAPK_ID,
                WEBAPK_START_URL_PATH,
                mTestServer.getURL(WEBAPK_SCOPE_PATH),
                null,
                WEBAPK_NAME,
                WEBAPK_SHORT_NAME,
                WebDisplayMode.Standalone,
                WEBAPK_ORIENTATION,
                0,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                false,
                "",
                mTestServer.getURL(WEBAPK_WEB_MANIFEST_URL));
    }

    /**
     * Navigates to {@link startUrl} and waits till the ManifestUpgradeDetector gets the Web
     * Manifest data.
     * @param info: the old Webapp info passed in to initialize the detector.
     * @param startUrl: URL to navigate to and start URL of the old Webapp info.
     */
    private void waitUntilManifestDataAvailable(WebappInfo info, String startUrl) throws Exception {
        final Tab tab = getActivity().getActivityTab();

        mDetector = new TestManifestUpgradeDetector(tab, info);
        mDetector.setOverrideMetadataForTesting(true);
        mDetector.setStartUrlForTesting(startUrl);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mDetector.start();
            }
        });

        CallbackHelper callbackHelper = mDetector.mCallbackHelper;
        int curCallCount = 0;
        new TabLoadObserver(tab).fullyLoadUrl(startUrl);
        callbackHelper.waitForCallback(curCallCount);
    }

    @MediumTest
    public void testManifestDoesNotUpgrade() throws Exception {
        waitUntilManifestDataAvailable(createWebappInfo(),
                mTestServer.getURL(WEBAPK_START_URL_PATH));

        assertFalse(mDetector.mIsUpgraded);
    }

    @MediumTest
    public void testStartUrlChangeShouldReturnUpgradeTrue() throws Exception {
        String currentStartUrl =
                "/chrome/test/data/webapps/manifest_test_page_test_start_url_change.html";
        waitUntilManifestDataAvailable(createWebappInfo(), mTestServer.getURL(currentStartUrl));

        assertTrue(mDetector.mIsUpgraded);
        assertEquals(mTestServer.getURL(WEBAPK_START_URL_PATH), mDetector.mStartUrl);
    }
}
