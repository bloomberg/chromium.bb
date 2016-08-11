// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.os.Bundle;

import org.chromium.base.ContextUtils;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.testing.local.LocalRobolectricTestRunner;

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
    private static final int WEBAPK_DISPLAY_MODE = WebDisplayMode.Standalone;
    private static final int WEBAPK_ORIENTATION = ScreenOrientationValues.LANDSCAPE;
    private static final long WEBAPK_THEME_COLOR = 1L;
    private static final long WEBAPK_BACKGROUND_COLOR = 2L;
    private static final String WEBAPK_MANIFEST_URL = "manifest.java";
    private static final String WEBAPK_PACKAGE_NAME = "package_name";

    private RobolectricPackageManager mPackageManager;

    /**
     * Data from downloaded Web Manifest. Same as old Web Manifest data by default.
     */
    private static class FetchedData {
        public String startUrl = WEBAPK_START_URL;
        public String scopeUrl = WEBAPK_SCOPE_URL;
        public String name = WEBAPK_NAME;
        public String shortName = WEBAPK_SHORT_NAME;
        public int displayMode = WEBAPK_DISPLAY_MODE;
        public int orientation = WEBAPK_ORIENTATION;
        public long themeColor = WEBAPK_THEME_COLOR;
        public long backgroundColor = WEBAPK_BACKGROUND_COLOR;
    }

    /**
     * ManifestUpgradeDetector subclass which:
     * - Stubs out ManifestUpgradeDetectorFetcher.
     * - Uses {@link fetchedData} passed into the constructor as the "Downloaded Manifest Data".
     * - Tracks whether an upgraded WebAPK was requested.
     * - Tracks whether "upgrade needed checking logic" has terminated.
     */
    private static class TestManifestUpgradeDetector extends ManifestUpgradeDetector {
        public boolean mIsUpgraded;
        public boolean mCompleted;
        private FetchedData mFetchedData;

        public TestManifestUpgradeDetector(Tab tab, WebappInfo info, FetchedData fetchedData) {
            super(tab, info);
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
                            mFetchedData.name, mFetchedData.shortName, mFetchedData.displayMode,
                            mFetchedData.orientation, mFetchedData.themeColor,
                            mFetchedData.backgroundColor);
                    return null;
                }
            };
            Mockito.doAnswer(mockStart).when(fetcher).start(
                    Mockito.any(ManifestUpgradeDetectorFetcher.Callback.class));
            return fetcher;
        }

        @Override
        protected void upgrade() {
            mIsUpgraded = true;
        }

        @Override
        protected void onComplete() {
            mCompleted = true;
        }
    }

    @Before
    public void setUp() {
        Context context = RuntimeEnvironment.application;
        ContextUtils.initApplicationContextForTests(context);
        mPackageManager = (RobolectricPackageManager) context.getPackageManager();

        setMetaData(WEBAPK_START_URL);
    }

    private TestManifestUpgradeDetector createDetector(FetchedData fetchedData) {
        WebappInfo webappInfo = WebappInfo.create("", WEBAPK_START_URL, WEBAPK_SCOPE_URL, null,
                WEBAPK_NAME, WEBAPK_SHORT_NAME, WEBAPK_DISPLAY_MODE, WEBAPK_ORIENTATION, 0,
                WEBAPK_THEME_COLOR, WEBAPK_BACKGROUND_COLOR, false, WEBAPK_PACKAGE_NAME,
                WEBAPK_MANIFEST_URL);
        return new TestManifestUpgradeDetector(null, webappInfo, fetchedData);
    }

    private void setMetaData(String startUrl) {
        Bundle bundle = new Bundle();
        bundle.putString(ManifestUpgradeDetector.META_DATA_START_URL, startUrl);

        ApplicationInfo appInfo = new ApplicationInfo();
        appInfo.metaData = bundle;

        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = WEBAPK_PACKAGE_NAME;
        packageInfo.applicationInfo = appInfo;
        mPackageManager.addPackage(packageInfo);
    }

    @Test
    public void testManifestDoesNotUpgrade() {
        TestManifestUpgradeDetector detector = createDetector(new FetchedData());
        detector.start();
        Assert.assertTrue(detector.mCompleted);
        Assert.assertFalse(detector.mIsUpgraded);
    }

    @Test
    public void testStartUrlChangeShouldUpgrade() {
        FetchedData fetchedData = new FetchedData();
        fetchedData.startUrl = "/changed.html";
        TestManifestUpgradeDetector detector = createDetector(fetchedData);
        detector.start();
        Assert.assertTrue(detector.mCompleted);
        Assert.assertTrue(detector.mIsUpgraded);
    }
}
