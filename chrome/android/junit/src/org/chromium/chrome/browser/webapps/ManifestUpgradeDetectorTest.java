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
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.HashMap;
import java.util.Map;

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
    private static final String WEBAPK_BEST_ICON_URL = "/icon.png";
    private static final String WEBAPK_BEST_ICON_MURMUR2_HASH = "3";
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
                boolean needsUpgrade, WebApkInfo info, String bestIconUrl) {}
        @Override
        public void onGotManifestData(boolean isUpgraded, WebApkInfo info, String bestIconUrl) {
            mIsUpgraded = isUpgraded;
            mWasCalled = true;
        }
    }

    private static class ManifestData {
        public String startUrl;
        public String scopeUrl;
        public String name;
        public String shortName;
        public Map<String, String> iconUrlToMurmur2HashMap;
        public String bestIconUrl;
        public Bitmap bestIcon;
        public int displayMode;
        public int orientation;
        public long themeColor;
        public long backgroundColor;
    }

    /**
     * ManifestUpgradeDetectorFetcher subclass which:
     * - Does not use native.
     * - Which returns the "Downloaded Manifest Data" passed to the constructor when
     *   {@link #start()} is called.
     */
    private static class TestManifestUpgradeDetectorFetcher extends ManifestUpgradeDetectorFetcher {
        ManifestData mFetchedManifestData;

        public TestManifestUpgradeDetectorFetcher(ManifestData fetchedManifestData) {
            mFetchedManifestData = fetchedManifestData;
        }

        @Override
        public boolean start(Tab tab, WebApkInfo oldInfo, Callback callback) {
            callback.onGotManifestData(
                    infoFromManifestData(mFetchedManifestData), mFetchedManifestData.bestIconUrl);
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
        private ManifestData mFetchedData;

        public TestManifestUpgradeDetector(WebApkInfo info, ManifestData fetchedData,
                ManifestUpgradeDetector.Callback callback) {
            super(null, info, callback);
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

    private static ManifestData defaultManifestData() {
        ManifestData manifestData = new ManifestData();
        manifestData.startUrl = WEBAPK_START_URL;
        manifestData.scopeUrl = WEBAPK_SCOPE_URL;
        manifestData.name = WEBAPK_NAME;
        manifestData.shortName = WEBAPK_SHORT_NAME;

        manifestData.iconUrlToMurmur2HashMap = new HashMap<String, String>();
        manifestData.iconUrlToMurmur2HashMap.put(
                WEBAPK_BEST_ICON_URL, WEBAPK_BEST_ICON_MURMUR2_HASH);

        manifestData.bestIconUrl = WEBAPK_BEST_ICON_URL;
        manifestData.bestIcon = createBitmap(Color.GREEN);
        manifestData.displayMode = WEBAPK_DISPLAY_MODE;
        manifestData.orientation = WEBAPK_ORIENTATION;
        manifestData.themeColor = WEBAPK_THEME_COLOR;
        manifestData.backgroundColor = WEBAPK_BACKGROUND_COLOR;
        return manifestData;
    }

    private static WebApkInfo infoFromManifestData(ManifestData manifestData) {
        return WebApkInfo.create("", "", manifestData.scopeUrl,
                new WebApkInfo.Icon(manifestData.bestIcon), manifestData.name,
                manifestData.shortName, manifestData.displayMode, manifestData.orientation, -1,
                manifestData.themeColor, manifestData.backgroundColor, WEBAPK_PACKAGE_NAME, -1,
                WEBAPK_MANIFEST_URL, manifestData.startUrl, manifestData.iconUrlToMurmur2HashMap);
    }

    private static TestManifestUpgradeDetector createDetectorWithFetchedData(
            ManifestData fetchedData, TestCallback callback) {
        return createDetector(defaultManifestData(), fetchedData, callback);
    }

    /**
     * Creates ManifestUpgradeDetector.
     * @param oldData Data used to create WebAPK. Potentially different from Web Manifest data at
     *                time that the WebAPK was generated.
     * @param fetchedData Data fetched by ManifestUpgradeDetector.
     * @param callback Callback to call when the upgrade check is complete.
     */
    private static TestManifestUpgradeDetector createDetector(
            ManifestData oldData, ManifestData fetchedData, TestCallback callback) {
        return new TestManifestUpgradeDetector(
                infoFromManifestData(oldData), fetchedData, callback);
    }

    @Test
    public void testManifestDoesNotUpgrade() {
        TestCallback callback = new TestCallback();
        TestManifestUpgradeDetector detector =
                createDetectorWithFetchedData(defaultManifestData(), callback);
        detector.start();
        Assert.assertTrue(callback.mWasCalled);
        Assert.assertFalse(callback.mIsUpgraded);
    }

    @Test
    public void testStartUrlChangeShouldUpgrade() {
        ManifestData fetchedData = defaultManifestData();
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
        ManifestData oldData = defaultManifestData();
        // webapk_installer.cc sets the scope to the default scope if the scope is empty.
        oldData.scopeUrl = ShortcutHelper.getScopeFromUrl(oldData.startUrl);
        ManifestData fetchedData = defaultManifestData();
        fetchedData.scopeUrl = "";
        Assert.assertTrue(!oldData.scopeUrl.equals(fetchedData.scopeUrl));

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
        ManifestData oldData = defaultManifestData();
        oldData.startUrl = "/fancy/scope/special/snowflake.html";
        oldData.scopeUrl = "/fancy/scope/";
        Assert.assertTrue(
                !oldData.scopeUrl.equals(ShortcutHelper.getScopeFromUrl(oldData.startUrl)));
        ManifestData fetchedData = defaultManifestData();
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
     * - WebAPK was generated using icon at {@link WEBAPK_BEST_ICON_URL} from Web Manifest.
     * - Bitmap at {@link WEBAPK_BEST_ICON_URL} has changed.
     */
    @Test
    public void testHomescreenIconChangeShouldUpgrade() {
        ManifestData fetchedData = defaultManifestData();
        fetchedData.iconUrlToMurmur2HashMap.put(fetchedData.bestIconUrl,
                WEBAPK_BEST_ICON_MURMUR2_HASH + "1");
        fetchedData.bestIcon = createBitmap(Color.BLUE);
        TestCallback callback = new TestCallback();
        TestManifestUpgradeDetector detector = createDetectorWithFetchedData(fetchedData, callback);

        detector.start();
        Assert.assertTrue(callback.mWasCalled);
        Assert.assertTrue(callback.mIsUpgraded);
    }

    /**
     * Test that an upgrade is requested when:
     * - WebAPK is generated using icon at {@link WEBAPK_BEST_ICON_URL} from Web Manifest.
     * - Web Manifest is updated to refer to different icon.
     */
    @Test
    public void testHomescreenBestIconUrlChangeShouldUpgrade() {
        ManifestData fetchedData = defaultManifestData();
        fetchedData.iconUrlToMurmur2HashMap.clear();
        fetchedData.iconUrlToMurmur2HashMap.put("/icon2.png", "22");
        fetchedData.bestIconUrl = "/icon2.png";

        TestCallback callback = new TestCallback();
        TestManifestUpgradeDetector detector = createDetectorWithFetchedData(fetchedData, callback);
        detector.start();
        Assert.assertTrue(callback.mWasCalled);
        Assert.assertTrue(callback.mIsUpgraded);
    }

    /**
     * Test that an upgrade is not requested if:
     * - icon URL is added to the Web Manifest
     * AND
     * - "best" icon URL for the launcher icon did not change.
     */
    @Test
    public void testIconUrlsChangeShouldNotUpgradeIfTheBestIconUrlDoesNotChange() {
        ManifestData fetchedData = defaultManifestData();
        fetchedData.iconUrlToMurmur2HashMap.clear();
        fetchedData.iconUrlToMurmur2HashMap.put(
                WEBAPK_BEST_ICON_URL, WEBAPK_BEST_ICON_MURMUR2_HASH);
        fetchedData.iconUrlToMurmur2HashMap.put("/icon2.png", null);

        TestCallback callback = new TestCallback();
        TestManifestUpgradeDetector detector = createDetectorWithFetchedData(fetchedData, callback);
        detector.start();
        Assert.assertTrue(callback.mWasCalled);
        Assert.assertFalse(callback.mIsUpgraded);
    }

    /**
     * Test than upgrade is requested if:
     * - the WebAPK's meta data has murmur2 hashes for all of the icons.
     * AND
     * - the Web Manifest has not changed
     * AND
     * - the computed best icon URL is different from the one stored in the WebAPK's meta data.
     */
    @Test
    public void testWebManifestSameButBestIconUrlChangedShouldNotUpgrade() {
        String iconUrl1 = "/icon1.png";
        String iconUrl2 = "/icon2.png";
        String hash1 = "11";
        String hash2 = "22";

        ManifestData oldData = defaultManifestData();
        oldData.bestIconUrl = iconUrl1;
        oldData.iconUrlToMurmur2HashMap.clear();
        oldData.iconUrlToMurmur2HashMap.put(iconUrl1, hash1);
        oldData.iconUrlToMurmur2HashMap.put(iconUrl2, hash2);

        ManifestData fetchedData = defaultManifestData();
        fetchedData.bestIconUrl = iconUrl2;
        fetchedData.iconUrlToMurmur2HashMap.clear();
        fetchedData.iconUrlToMurmur2HashMap.put(iconUrl1, null);
        fetchedData.iconUrlToMurmur2HashMap.put(iconUrl2, hash2);

        TestCallback callback = new TestCallback();
        TestManifestUpgradeDetector detector = createDetector(oldData, fetchedData, callback);
        detector.start();
        Assert.assertTrue(callback.mWasCalled);
        Assert.assertFalse(callback.mIsUpgraded);
    }
}
