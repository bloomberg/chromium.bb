// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.chrome.browser.webapps.ManifestUpgradeDetector.FetchedManifestData;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.text.TextUtils;

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

import org.chromium.base.ContextUtils;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.testing.local.LocalRobolectricTestRunner;

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
                boolean needsUpgrade, ManifestUpgradeDetector.FetchedManifestData data) {}
        @Override
        public void onGotManifestData(boolean isUpgraded, FetchedManifestData data) {
            mIsUpgraded = isUpgraded;
            mWasCalled = true;
        }
    }

    /**
     * The WebappInfoCreationData is the data extracted from a WebAPK. It is used to create a
     * WebappInfo for WebAPKs.
     */
    private static class WebappInfoCreationData extends FetchedManifestData {}

    /**
     * ManifestUpgradeDetector subclass which:
     * - Stubs out ManifestUpgradeDetectorFetcher.
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
        populateDataWithDefaults(data);
        return data;
    }

    /**
     * Create a default data. The WebappInfoCreationData is the data that the WebAPK was created
     * with.
     */
    private WebappInfoCreationData createDefaultWebappInfoCreationData() {
        WebappInfoCreationData data = new WebappInfoCreationData();
        populateDataWithDefaults(data);
        return data;
    }

    private void populateDataWithDefaults(FetchedManifestData data) {
        data.startUrl = WEBAPK_START_URL;
        data.scopeUrl = WEBAPK_SCOPE_URL;
        data.name = WEBAPK_NAME;
        data.shortName = WEBAPK_SHORT_NAME;
        data.iconUrl = WEBAPK_ICON_URL;
        data.iconMurmur2Hash = WEBAPK_ICON_MURMUR2_HASH;
        data.icon = createBitmap(Color.GREEN);
        data.displayMode = WEBAPK_DISPLAY_MODE;
        data.orientation = WEBAPK_ORIENTATION;
        data.themeColor = WEBAPK_THEME_COLOR;
        data.backgroundColor = WEBAPK_BACKGROUND_COLOR;
    }

    private TestManifestUpgradeDetector createDetectorWithFetchedData(
            FetchedManifestData fetchedData, TestCallback callback) {
        return createDetector(createDefaultWebappInfoCreationData(), fetchedData, callback);
    }

    /**
     * Creates ManifestUpgradeDetector.
     * @param oldData Data used to create WebAPK. Potentially different from Web Manifest data at
     *                time that the WebAPK was generated.
     * @param fetchedData Data fetched by ManifestUpgradeDetector.
     * @param callback Callback to call when the upgrade check is complete.
     */
    private TestManifestUpgradeDetector createDetector(WebappInfoCreationData oldData,
            FetchedManifestData fetchedData, TestCallback callback) {
        WebApkMetaData metaData = new WebApkMetaData();
        metaData.manifestUrl = WEBAPK_MANIFEST_URL;
        metaData.startUrl = oldData.startUrl;
        metaData.scope = oldData.scopeUrl;
        metaData.name = oldData.name;
        metaData.shortName = oldData.shortName;
        metaData.displayMode = oldData.displayMode;
        metaData.orientation = oldData.orientation;
        metaData.themeColor = oldData.themeColor;
        metaData.backgroundColor = oldData.backgroundColor;
        metaData.iconUrl = oldData.iconUrl;
        metaData.iconMurmur2Hash = oldData.iconMurmur2Hash;
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
        WebappInfoCreationData oldData = createDefaultWebappInfoCreationData();
        // webapk_installer.cc sets the scope to the default scope if the scope is empty.
        oldData.scopeUrl = ShortcutHelper.getScopeFromUrl(oldData.startUrl);
        FetchedManifestData fetchedData = createDefaultFetchedManifestData();
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
        WebappInfoCreationData oldData = createDefaultWebappInfoCreationData();
        oldData.startUrl = "/fancy/scope/special/snowflake.html";
        oldData.scopeUrl = "/fancy/scope/";
        Assert.assertTrue(
                !oldData.scopeUrl.equals(ShortcutHelper.getScopeFromUrl(oldData.startUrl)));
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
        fetchedData.iconMurmur2Hash = WEBAPK_ICON_MURMUR2_HASH + "1";
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
        FetchedManifestData fetchedData = createDefaultFetchedManifestData();
        fetchedData.iconUrl = "/icon2.png";

        TestCallback callback = new TestCallback();
        TestManifestUpgradeDetector detector = createDetectorWithFetchedData(fetchedData, callback);
        detector.start();
        Assert.assertTrue(callback.mWasCalled);
        Assert.assertTrue(callback.mIsUpgraded);
    }
}
