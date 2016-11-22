// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.text.TextUtils;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.res.builder.RobolectricPackageManager;
import org.robolectric.shadows.ShadowBitmap;

import org.chromium.base.ContextUtils;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.ManifestUpgradeDetectorFetcher.FetchedManifestData;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.HashMap;

/**
 * Tests the ManifestUpgradeDetector.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ManifestUpgradeDetectorTest {

    private static final String WEBAPK_START_URL = "/start_url.html";
    private static final String WEBAPK_SCOPE_URL = "/";
    private static final String WEBAPK_NAME = "Long Name";
    private static final String WEBAPK_SHORT_NAME = "Short Name";
    private static final String WEBAPK_ICON_URL = "/icon.png";
    private static final String WEBAPK_ICON_MURMUR2_HASH = "3";
    private static final int WEBAPK_DISPLAY_MODE = WebDisplayMode.Standalone;
    private static final int WEBAPK_ORIENTATION = ScreenOrientationValues.LANDSCAPE;
    private static final long WEBAPK_THEME_COLOR = 1L;
    private static final long WEBAPK_BACKGROUND_COLOR = 2L;
    private static final String WEBAPK_MANIFEST_URL = "manifest.json";
    private static final String WEBAPK_PACKAGE_NAME = "package_name";

    private RobolectricPackageManager mPackageManager;

    private static class TestCallback implements ManifestUpgradeDetector.Callback {
        public boolean mIsUpgraded;
        public boolean mWasCalled;
        @Override
        public void onFinishedFetchingWebManifestForInitialUrl(
                boolean needsUpgrade, FetchedManifestData data) {}
        @Override
        public void onGotManifestData(boolean isUpgraded, FetchedManifestData data) {
            mIsUpgraded = isUpgraded;
            mWasCalled = true;
        }
    }

    /**
     * ManifestUpgradeDetectorFetcher subclass which:
     * - Does not use native.
     * - Which returns the "Downloaded Manifest Data" passed to the constructor when
     *   {@link #start()} is called.
     */
    private static class TestManifestUpgradeDetectorFetcher extends ManifestUpgradeDetectorFetcher {
        FetchedManifestData mFetchedManifestData;

        public TestManifestUpgradeDetectorFetcher(FetchedManifestData fetchedManifestData) {
            mFetchedManifestData = fetchedManifestData;
        }

        @Override
        public boolean start(Tab tab, String scopeUrl, String manifestUrl, Callback callback) {
            mCallback = callback;

            // Call {@link #onDataAvailable()} instead of the callback because
            // {@link #onDataAvailable()} sets defaults which we want to test.
            onDataAvailable(mFetchedManifestData.startUrl, mFetchedManifestData.scopeUrl,
                    mFetchedManifestData.name, mFetchedManifestData.shortName,
                    mFetchedManifestData.bestIconUrl, mFetchedManifestData.bestIconMurmur2Hash,
                    mFetchedManifestData.bestIcon, mFetchedManifestData.iconUrls,
                    mFetchedManifestData.displayMode, mFetchedManifestData.orientation,
                    mFetchedManifestData.themeColor, mFetchedManifestData.backgroundColor);
            return true;
        }

        @Override
        public void destroy() {
            // Do nothing.
        }
    }

    /**
     * ManifestUpgradeDetector subclass which:
     * - Uses mock ManifestUpgradeDetectorFetcher.
     * - Uses {@link fetchedData} passed into the constructor as the "Downloaded Manifest Data".
     * - Tracks whether an upgraded WebAPK was requested.
     * - Tracks whether "upgrade needed checking logic" has terminated.
     */
    private static class TestManifestUpgradeDetector extends ManifestUpgradeDetector {
        private FetchedManifestData mFetchedData;

        public TestManifestUpgradeDetector(Tab tab, WebApkMetaData metaData,
                FetchedManifestData fetchedData, ManifestUpgradeDetector.Callback callback) {
            super(tab, metaData, callback);
            mFetchedData = fetchedData;
        }

        @Override
        public ManifestUpgradeDetectorFetcher createFetcher() {
            return new TestManifestUpgradeDetectorFetcher(mFetchedData);
        }

        // Stubbed out because real implementation uses native.
        @Override
        protected boolean urlsMatchIgnoringFragments(String url1, String url2) {
            return TextUtils.equals(url1, url2);
        }
    }

    /**
     * Creates 1x1 bitmap.
     * @param color The bitmap color.
     */
    private static Bitmap createBitmap(int color) {
        int colors[] = { color };
        return ShadowBitmap.createBitmap(colors, 1, 1, Bitmap.Config.ALPHA_8);
    }

    @Before
    public void setUp() {
        Context context = RuntimeEnvironment.application;
        ContextUtils.initApplicationContextForTests(context);
        mPackageManager = (RobolectricPackageManager) context.getPackageManager();
    }

    /**
     * Create a default data. The FetchedManifestData is the data fetched by
     * ManifestUpgradeDetector.
     */
    private FetchedManifestData createDefaultFetchedManifestData() {
        FetchedManifestData data = new FetchedManifestData();
        data.startUrl = WEBAPK_START_URL;
        data.scopeUrl = WEBAPK_SCOPE_URL;
        data.name = WEBAPK_NAME;
        data.shortName = WEBAPK_SHORT_NAME;
        data.bestIconUrl = WEBAPK_ICON_URL;
        data.bestIconMurmur2Hash = WEBAPK_ICON_MURMUR2_HASH;
        data.bestIcon = createBitmap(Color.GREEN);
        data.iconUrls = new String[] { data.bestIconUrl };
        data.displayMode = WEBAPK_DISPLAY_MODE;
        data.orientation = WEBAPK_ORIENTATION;
        data.themeColor = WEBAPK_THEME_COLOR;
        data.backgroundColor = WEBAPK_BACKGROUND_COLOR;
        return data;
    }

    /**
     * Create a default data. The WebApkMetaData is the data that the WebAPK was created
     * with.
     */
    private WebApkMetaData createDefaultWebApkMetaData() {
        WebApkMetaData data = new WebApkMetaData();
        data.startUrl = WEBAPK_START_URL;
        data.scope = WEBAPK_SCOPE_URL;
        data.name = WEBAPK_NAME;
        data.shortName = WEBAPK_SHORT_NAME;
        data.displayMode = WEBAPK_DISPLAY_MODE;
        data.orientation = WEBAPK_ORIENTATION;
        data.themeColor = WEBAPK_THEME_COLOR;
        data.backgroundColor = WEBAPK_BACKGROUND_COLOR;

        data.iconUrlAndIconMurmur2HashMap = new HashMap<String, String>();
        data.iconUrlAndIconMurmur2HashMap.put(WEBAPK_ICON_URL, WEBAPK_ICON_MURMUR2_HASH);

        return data;
    }

    private TestManifestUpgradeDetector createDetectorWithFetchedData(
            FetchedManifestData fetchedData, TestCallback callback) {
        return createDetector(createDefaultWebApkMetaData(), fetchedData, callback);
    }

    /**
     * Creates ManifestUpgradeDetector.
     * @param oldData Data used to create WebAPK. Potentially different from Web Manifest data at
     *                time that the WebAPK was generated.
     * @param fetchedData Data fetched by ManifestUpgradeDetector.
     * @param callback Callback to call when the upgrade check is complete.
     */
    private TestManifestUpgradeDetector createDetector(WebApkMetaData oldData,
            FetchedManifestData fetchedData, TestCallback callback) {
        WebApkMetaData metaData = new WebApkMetaData();
        metaData.manifestUrl = WEBAPK_MANIFEST_URL;
        metaData.startUrl = oldData.startUrl;
        metaData.scope = oldData.scope;
        metaData.name = oldData.name;
        metaData.shortName = oldData.shortName;
        metaData.displayMode = oldData.displayMode;
        metaData.orientation = oldData.orientation;
        metaData.themeColor = oldData.themeColor;
        metaData.backgroundColor = oldData.backgroundColor;
        metaData.iconUrlAndIconMurmur2HashMap = new HashMap<String, String>(
                oldData.iconUrlAndIconMurmur2HashMap);
        return new TestManifestUpgradeDetector(null, metaData, fetchedData, callback);
    }

    @Test
    public void testManifestDoesNotUpgrade() {
        TestCallback callback = new TestCallback();
        TestManifestUpgradeDetector detector = createDetectorWithFetchedData(
                createDefaultFetchedManifestData(), callback);
        detector.start();
        Assert.assertTrue(callback.mWasCalled);
        Assert.assertFalse(callback.mIsUpgraded);
    }

    @Test
    public void testStartUrlChangeShouldUpgrade() {
        FetchedManifestData fetchedData = createDefaultFetchedManifestData();
        fetchedData.startUrl = "/changed.html";
        TestCallback callback = new TestCallback();
        TestManifestUpgradeDetector detector = createDetectorWithFetchedData(fetchedData, callback);
        detector.start();
        Assert.assertTrue(callback.mWasCalled);
        Assert.assertTrue(callback.mIsUpgraded);
    }

    /**
     * Test that an upgrade is not requested when the Web Manifest did not change and the Web
     * Manifest scope is empty.
     */
    @Test
    public void testManifestEmptyScopeShouldNotUpgrade() {
        WebApkMetaData oldData = createDefaultWebApkMetaData();
        // webapk_installer.cc sets the scope to the default scope if the scope is empty.
        oldData.scope = ShortcutHelper.getScopeFromUrl(oldData.startUrl);
        FetchedManifestData fetchedData = createDefaultFetchedManifestData();
        fetchedData.scopeUrl = "";
        Assert.assertTrue(!oldData.scope.equals(fetchedData.scopeUrl));

        TestCallback callback = new TestCallback();
        TestManifestUpgradeDetector detector = createDetector(oldData, fetchedData, callback);
        detector.start();
        Assert.assertTrue(callback.mWasCalled);
        Assert.assertFalse(callback.mIsUpgraded);
    }

    /**
     * Test that an upgrade is requested when the Web Manifest is updated from using a non-empty
     * scope to an empty scope.
     */
    @Test
    public void testManifestNonEmptyScopeToEmptyScopeShouldUpgrade() {
        WebApkMetaData oldData = createDefaultWebApkMetaData();
        oldData.startUrl = "/fancy/scope/special/snowflake.html";
        oldData.scope = "/fancy/scope/";
        Assert.assertTrue(
                !oldData.scope.equals(ShortcutHelper.getScopeFromUrl(oldData.startUrl)));
        FetchedManifestData fetchedData = createDefaultFetchedManifestData();
        fetchedData.startUrl = "/fancy/scope/special/snowflake.html";
        fetchedData.scopeUrl = "";

        TestCallback callback = new TestCallback();
        TestManifestUpgradeDetector detector = createDetector(oldData, fetchedData, callback);
        detector.start();
        Assert.assertTrue(callback.mWasCalled);
        Assert.assertTrue(callback.mIsUpgraded);
    }

    /**
     * Test that an upgrade is requested when:
     * - WebAPK was generated using icon at {@link WEBAPK_ICON_URL} from Web Manifest.
     * - Bitmap at {@link WEBAPK_ICON_URL} has changed.
     */
    @Test
    public void testHomescreenIconChangeShouldUpgrade() {
        FetchedManifestData fetchedData = createDefaultFetchedManifestData();
        fetchedData.bestIconMurmur2Hash = WEBAPK_ICON_MURMUR2_HASH + "1";
        fetchedData.bestIcon = createBitmap(Color.BLUE);
        TestCallback callback = new TestCallback();
        TestManifestUpgradeDetector detector = createDetectorWithFetchedData(fetchedData, callback);

        detector.start();
        Assert.assertTrue(callback.mWasCalled);
        Assert.assertTrue(callback.mIsUpgraded);
    }

    /**
     * Test that an upgrade is requested when:
     * - WebAPK is generated using icon at {@link WEBAPK_ICON_URL} from Web Manifest.
     * - Web Manifest is updated to refer to different icon.
     */
    @Test
    public void testHomescreenBestIconUrlChangeShouldUpgrade() {
        FetchedManifestData fetchedData = createDefaultFetchedManifestData();
        fetchedData.bestIconUrl = "/icon2.png";

        TestCallback callback = new TestCallback();
        TestManifestUpgradeDetector detector = createDetectorWithFetchedData(fetchedData, callback);
        detector.start();
        Assert.assertTrue(callback.mWasCalled);
        Assert.assertTrue(callback.mIsUpgraded);
    }

    /** Test that an upgrade is requested when a new icon URL is added to the Web Manifest. */
    @Test
    public void testIconUrlsChangeShouldNotUpgradeIfTheBestIconUrlDoesNotChange() {
        FetchedManifestData fetchedData = createDefaultFetchedManifestData();
        fetchedData.iconUrls = new String[] { fetchedData.bestIconUrl, "/icon2.png" };

        TestCallback callback = new TestCallback();
        TestManifestUpgradeDetector detector = createDetectorWithFetchedData(fetchedData, callback);
        detector.start();
        Assert.assertTrue(callback.mWasCalled);
        Assert.assertFalse(callback.mIsUpgraded);
    }

    /**
     * Test that an upgrade isn't requested when the Web Manifest has multiple icons and the Web
     * Manifest has not changed.
     */
    @Test
    public void testNoIconUrlsChangeShouldNotUpgrade() {
        String icon1 = "/icon1.png";
        String icon2 = "/icon2.png";
        String hash1 = "11";
        String hash2 = "22";
        WebApkMetaData oldData = createDefaultWebApkMetaData();
        oldData.iconUrlAndIconMurmur2HashMap.put(icon1, hash1);
        oldData.iconUrlAndIconMurmur2HashMap.put(icon2, hash2);

        FetchedManifestData fetchedData = createDefaultFetchedManifestData();
        fetchedData.iconUrls = new String[] {fetchedData.bestIconUrl, icon1, icon2 };
        fetchedData.bestIconUrl = icon2;
        fetchedData.bestIconMurmur2Hash = hash2;
        TestCallback callback = new TestCallback();
        TestManifestUpgradeDetector detector = createDetector(oldData, fetchedData, callback);
        detector.start();
        Assert.assertTrue(callback.mWasCalled);
        Assert.assertFalse(callback.mIsUpgraded);
    }
}
