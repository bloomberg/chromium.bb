// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;

import org.chromium.base.ContextUtils;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.res.builder.RobolectricPackageManager;
import org.robolectric.shadows.ShadowBitmap;

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
    private static final long WEBAPK_ICON_MURMUR2_HASH = 3L;
    private static final int WEBAPK_DISPLAY_MODE = WebDisplayMode.Standalone;
    private static final int WEBAPK_ORIENTATION = ScreenOrientationValues.LANDSCAPE;
    private static final long WEBAPK_THEME_COLOR = 1L;
    private static final long WEBAPK_BACKGROUND_COLOR = 2L;
    private static final String WEBAPK_MANIFEST_URL = "manifest.json";
    private static final String WEBAPK_PACKAGE_NAME = "package_name";

    private RobolectricPackageManager mPackageManager;

    /**
     * Used to represent either:
     * - Data that the WebAPK was created with.
     * OR
     * - Data fetched by ManifestUpgradeDetector.
     */
    private static class Data {
        public String startUrl = WEBAPK_START_URL;
        public String scopeUrl = WEBAPK_SCOPE_URL;
        public String name = WEBAPK_NAME;
        public String shortName = WEBAPK_SHORT_NAME;
        public String iconUrl = WEBAPK_ICON_URL;
        public long iconMurmur2Hash = WEBAPK_ICON_MURMUR2_HASH;
        public Bitmap icon = createBitmap(Color.GREEN);
        public int displayMode = WEBAPK_DISPLAY_MODE;
        public int orientation = WEBAPK_ORIENTATION;
        public long themeColor = WEBAPK_THEME_COLOR;
        public long backgroundColor = WEBAPK_BACKGROUND_COLOR;
    }

    private static class TestCallback implements ManifestUpgradeDetector.Callback {
        public boolean mIsUpgraded;
        public boolean mWasCalled;
        @Override
        public void onUpgradeNeededCheckFinished(boolean isUpgraded, WebappInfo newInfo) {
            mIsUpgraded = isUpgraded;
            mWasCalled = true;
        }
    }

    /**
     * ManifestUpgradeDetector subclass which:
     * - Stubs out ManifestUpgradeDetectorFetcher.
     * - Uses {@link fetchedData} passed into the constructor as the "Downloaded Manifest Data".
     * - Tracks whether an upgraded WebAPK was requested.
     * - Tracks whether "upgrade needed checking logic" has terminated.
     */
    private static class TestManifestUpgradeDetector extends ManifestUpgradeDetector {
        private Data mFetchedData;

        public TestManifestUpgradeDetector(Tab tab, WebappInfo info, Data fetchedData,
                ManifestUpgradeDetector.Callback callback) {
            super(tab, info, callback);
            mFetchedData = fetchedData;
        }

        @Override
        public ManifestUpgradeDetectorFetcher createFetcher(
                Tab tab, String scopeUrl, String manifestUrl) {
            ManifestUpgradeDetectorFetcher fetcher =
                    Mockito.mock(ManifestUpgradeDetectorFetcher.class);
            Answer<Void> mockStart = new Answer<Void>() {
                public Void answer(InvocationOnMock invocation) throws Throwable {
                    ManifestUpgradeDetectorFetcher.Callback callback =
                            (ManifestUpgradeDetectorFetcher.Callback) invocation.getArguments()[0];
                    callback.onGotManifestData(mFetchedData.startUrl, mFetchedData.scopeUrl,
                            mFetchedData.name, mFetchedData.shortName, mFetchedData.iconUrl,
                            mFetchedData.iconMurmur2Hash, mFetchedData.icon,
                            mFetchedData.displayMode, mFetchedData.orientation,
                            mFetchedData.themeColor, mFetchedData.backgroundColor);
                    return null;
                }
            };
            Mockito.doAnswer(mockStart).when(fetcher).start(
                    Mockito.any(ManifestUpgradeDetectorFetcher.Callback.class));
            return fetcher;
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

    private TestManifestUpgradeDetector createDetectorWithFetchedData(Data fetchedData,
            TestCallback callback) {
        return createDetector(new Data(), fetchedData, callback);
    }

    /**
     * Creates ManifestUpgradeDetector.
     * @param oldData Data used to create WebAPK. Potentially different from Web Manifest data at
     *                time that the WebAPK was generated.
     * @param fetchedData Data fetched by ManifestUpgradeDetector.
     * @param callback Callback to call when the upgrade check is complete.
     */
    private TestManifestUpgradeDetector createDetector(Data oldData, Data fetchedData,
            TestCallback callback) {
        setMetaData(
                WEBAPK_MANIFEST_URL, oldData.startUrl, oldData.iconUrl, oldData.iconMurmur2Hash);
        WebappInfo webappInfo = WebappInfo.create("", oldData.startUrl, oldData.scopeUrl, null,
                oldData.name, oldData.shortName, oldData.displayMode, oldData.orientation, 0,
                oldData.themeColor, oldData.backgroundColor, false, WEBAPK_PACKAGE_NAME);
        return new TestManifestUpgradeDetector(null, webappInfo, fetchedData, callback);
    }

    private void setMetaData(
            String manifestUrl, String startUrl, String iconUrl, long iconMurmur2Hash) {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.WEB_MANIFEST_URL, manifestUrl);
        bundle.putString(WebApkMetaDataKeys.START_URL, startUrl);
        bundle.putString(WebApkMetaDataKeys.ICON_URL, iconUrl);
        bundle.putString(WebApkMetaDataKeys.ICON_MURMUR2_HASH, iconMurmur2Hash + "L");

        ApplicationInfo appInfo = new ApplicationInfo();
        appInfo.metaData = bundle;

        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = WEBAPK_PACKAGE_NAME;
        packageInfo.applicationInfo = appInfo;
        mPackageManager.addPackage(packageInfo);
    }

    @Test
    public void testManifestDoesNotUpgrade() {
        TestCallback callback = new TestCallback();
        TestManifestUpgradeDetector detector = createDetectorWithFetchedData(new Data(), callback);
        detector.start();
        Assert.assertTrue(callback.mWasCalled);
        Assert.assertFalse(callback.mIsUpgraded);
    }

    @Test
    public void testStartUrlChangeShouldUpgrade() {
        Data fetchedData = new Data();
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
        Data oldData = new Data();
        // webapk_installer.cc sets the scope to the default scope if the scope is empty.
        oldData.scopeUrl = ShortcutHelper.getScopeFromUrl(oldData.startUrl);
        Data fetchedData = new Data();
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
        Data oldData = new Data();
        oldData.startUrl = "/fancy/scope/special/snowflake.html";
        oldData.scopeUrl = "/fancy/scope/";
        Assert.assertTrue(
                !oldData.scopeUrl.equals(ShortcutHelper.getScopeFromUrl(oldData.startUrl)));
        Data fetchedData = new Data();
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
        Data fetchedData = new Data();
        fetchedData.iconMurmur2Hash = WEBAPK_ICON_MURMUR2_HASH + 1;
        fetchedData.icon = createBitmap(Color.BLUE);
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
    public void testHomescreenIconUrlChangeShouldUpgrade() {
        Data fetchedData = new Data();
        fetchedData.iconUrl = "/icon2.png";

        TestCallback callback = new TestCallback();
        TestManifestUpgradeDetector detector = createDetectorWithFetchedData(fetchedData, callback);
        detector.start();
        Assert.assertTrue(callback.mWasCalled);
        Assert.assertTrue(callback.mIsUpgraded);
    }

    /**
     * Test that an upgrade is not requested when:
     * - WebAPK was generated using icon at {@link WEBAPK_ICON_URL} from Web Manifest.
     * - Web Manifest was updated and now does not contain any icon URLs.
     */
    @Test
    public void testHomescreenIconUrlsRemovedShouldNotUpgrade() {
        Data fetchedData = new Data();
        fetchedData.iconUrl = "";
        fetchedData.iconMurmur2Hash = 0L;
        fetchedData.icon = null;

        TestCallback callback = new TestCallback();
        TestManifestUpgradeDetector detector = createDetectorWithFetchedData(fetchedData, callback);
        detector.start();
        Assert.assertTrue(callback.mWasCalled);
        Assert.assertFalse(callback.mIsUpgraded);
    }
}
