// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.provider.Settings;
import android.text.TextUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowBitmap;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.DisableHistogramsRule;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.webapk.lib.client.WebApkVersion;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

import java.util.HashMap;
import java.util.Map;

/**
 * Unit tests for WebApkUpdateManager.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebApkUpdateManagerTest {
    @Rule
    public DisableHistogramsRule mDisableHistogramsRule = new DisableHistogramsRule();

    /** WebAPK's id in {@link WebAppDataStorage}. */
    private static final String WEBAPK_ID =
            WebApkConstants.WEBAPK_ID_PREFIX + WebApkTestHelper.WEBAPK_PACKAGE_NAME;

    /** Web Manifest URL */
    private static final String WEB_MANIFEST_URL = "manifest.json";

    private static final String START_URL = "/start_url.html";
    private static final String SCOPE_URL = "/";
    private static final String NAME = "Long Name";
    private static final String SHORT_NAME = "Short Name";
    private static final String ICON_URL = "/icon.png";
    private static final String ICON_MURMUR2_HASH = "3";
    private static final int DISPLAY_MODE = WebDisplayMode.Undefined;
    private static final int ORIENTATION = ScreenOrientationValues.DEFAULT;
    private static final long THEME_COLOR = 1L;
    private static final long BACKGROUND_COLOR = 2L;

    /** Different name than the one used in {@link defaultManifestData()}. */
    private static final String DIFFERENT_NAME = "Different Name";

    /** {@link WebappDataStorage#Clock} subclass which enables time to be manually advanced. */
    private static class MockClock extends WebappDataStorage.Clock {
        // 0 has a special meaning: {@link WebappDataStorage#LAST_USED_UNSET}.
        private long mTimeMillis = 1;

        public void advance(long millis) {
            mTimeMillis += millis;
        }

        @Override
        public long currentTimeMillis() {
            return mTimeMillis;
        }
    }

    /** Mock {@link WebApkUpdateDataFetcher}. */
    private static class TestWebApkUpdateDataFetcher extends WebApkUpdateDataFetcher {
        private boolean mStarted;

        public boolean wasStarted() {
            return mStarted;
        }

        @Override
        public boolean start(Tab tab, WebApkInfo oldInfo, Observer observer) {
            mStarted = true;
            return true;
        }
    }

    private static class TestWebApkUpdateManager extends WebApkUpdateManager {
        private WebappDataStorage.Clock mClock;
        private TestWebApkUpdateDataFetcher mFetcher;
        private boolean mUpdateRequested;
        private String mUpdateName;
        private boolean mDestroyedFetcher;
        private boolean mIsWebApkForeground;

        public TestWebApkUpdateManager(WebappDataStorage.Clock clock, WebappDataStorage storage) {
            super(null, storage);
            mClock = clock;
        }

        /**
         * Returns whether the is-update-needed check has been triggered.
         */
        public boolean updateCheckStarted() {
            return mFetcher != null && mFetcher.wasStarted();
        }

        /**
         * Returns whether an update has been requested.
         */
        public boolean updateRequested() {
            return mUpdateRequested;
        }

        /**
         * Returns the "name" from the requested update. Null if an update has not been requested.
         */
        public String requestedUpdateName() {
            return mUpdateName;
        }

        public boolean destroyedFetcher() {
            return mDestroyedFetcher;
        }

        @Override
        protected WebApkUpdateDataFetcher buildFetcher() {
            mFetcher = new TestWebApkUpdateDataFetcher();
            return mFetcher;
        }

        @Override
        protected void scheduleUpdate(WebApkInfo info, String bestIconUrl,
                boolean isManifestStale) {
            mUpdateName = info.name();
            super.scheduleUpdate(info, bestIconUrl, isManifestStale);
        }

        @Override
        protected void updateAsyncImpl(WebApkInfo info, String bestIconUrl,
                boolean isManifestStale) {
            mUpdateRequested = true;
        }

        @Override
        protected boolean isInForeground() {
            return mIsWebApkForeground;
        }

        @Override
        protected void destroyFetcher() {
            mFetcher = null;
            mDestroyedFetcher = true;
        }

        @Override
        protected long currentTimeMillis() {
            return mClock.currentTimeMillis();
        }

        public void setIsWebApkForeground(boolean isForeground) {
            mIsWebApkForeground = isForeground;
        }

        // Stubbed out because real implementation uses native.
        @Override
        protected boolean urlsMatchIgnoringFragments(String url1, String url2) {
            return TextUtils.equals(url1, url2);
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

    private MockClock mClock;

    private WebappDataStorage getStorage() {
        return WebappRegistry.getInstance().getWebappDataStorage(WEBAPK_ID);
    }

    /**
     * Registers WebAPK with default package name. Overwrites previous registrations.
     * @param manifestData        <meta-data> values for WebAPK's Android Manifest.
     * @param shellApkVersionCode WebAPK's version of the //chrome/android/webapk/shell_apk code.
     */
    private void registerWebApk(ManifestData manifestData, int shellApkVersionCode) {
        Bundle metaData = new Bundle();
        metaData.putInt(
                WebApkMetaDataKeys.SHELL_APK_VERSION, shellApkVersionCode);
        metaData.putString(WebApkMetaDataKeys.START_URL, manifestData.startUrl);
        metaData.putString(WebApkMetaDataKeys.SCOPE, manifestData.scopeUrl);
        metaData.putString(WebApkMetaDataKeys.NAME, manifestData.name);
        metaData.putString(WebApkMetaDataKeys.SHORT_NAME, manifestData.shortName);
        metaData.putString(WebApkMetaDataKeys.THEME_COLOR, manifestData.themeColor + "L");
        metaData.putString(WebApkMetaDataKeys.BACKGROUND_COLOR, manifestData.backgroundColor + "L");
        metaData.putString(WebApkMetaDataKeys.WEB_MANIFEST_URL, WEB_MANIFEST_URL);

        String iconUrlsAndIconMurmur2Hashes = "";
        for (Map.Entry<String, String> mapEntry : manifestData.iconUrlToMurmur2HashMap.entrySet()) {
            String murmur2Hash = mapEntry.getValue();
            if (murmur2Hash == null) {
                murmur2Hash = "0";
            }
            iconUrlsAndIconMurmur2Hashes += " " + mapEntry.getKey() + " " + murmur2Hash;
        }
        iconUrlsAndIconMurmur2Hashes = iconUrlsAndIconMurmur2Hashes.trim();
        metaData.putString(WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES,
                iconUrlsAndIconMurmur2Hashes);

        WebApkTestHelper.registerWebApkWithMetaData(metaData);
    }

    private static ManifestData defaultManifestData() {
        ManifestData manifestData = new ManifestData();
        manifestData.startUrl = START_URL;
        manifestData.scopeUrl = SCOPE_URL;
        manifestData.name = NAME;
        manifestData.shortName = SHORT_NAME;

        manifestData.iconUrlToMurmur2HashMap = new HashMap<String, String>();
        manifestData.iconUrlToMurmur2HashMap.put(ICON_URL, ICON_MURMUR2_HASH);

        manifestData.bestIconUrl = ICON_URL;
        manifestData.bestIcon = createBitmap(Color.GREEN);
        manifestData.displayMode = DISPLAY_MODE;
        manifestData.orientation = ORIENTATION;
        manifestData.themeColor = THEME_COLOR;
        manifestData.backgroundColor = BACKGROUND_COLOR;
        return manifestData;
    }

    private static WebApkInfo infoFromManifestData(ManifestData manifestData) {
        if (manifestData == null) return null;

        return WebApkInfo.create(WEBAPK_ID, "", manifestData.scopeUrl,
                new WebApkInfo.Icon(manifestData.bestIcon), manifestData.name,
                manifestData.shortName, manifestData.displayMode, manifestData.orientation, -1,
                manifestData.themeColor, manifestData.backgroundColor,
                WebApkTestHelper.WEBAPK_PACKAGE_NAME, -1, WEB_MANIFEST_URL,
                manifestData.startUrl, manifestData.iconUrlToMurmur2HashMap);
    }

    /**
     * Creates 1x1 bitmap.
     * @param color The bitmap color.
     */
    private static Bitmap createBitmap(int color) {
        int colors[] = { color };
        return ShadowBitmap.createBitmap(colors, 1, 1, Bitmap.Config.ALPHA_8);
    }

    private static void updateIfNeeded(WebApkUpdateManager updateManager) {
        // Use the intent version of {@link WebApkInfo#create()} in order to test default values
        // set by the intent version of {@link WebApkInfo#create()}.
        Intent intent = new Intent();
        intent.putExtra(ShortcutHelper.EXTRA_URL, "");
        intent.putExtra(
                ShortcutHelper.EXTRA_WEBAPK_PACKAGE_NAME, WebApkTestHelper.WEBAPK_PACKAGE_NAME);
        WebApkInfo info = WebApkInfo.create(intent);

        updateManager.updateIfNeeded(null, info);
    }

    private static void onGotUnchangedWebManifestData(WebApkUpdateManager updateManager) {
        onGotManifestData(updateManager, defaultManifestData());
    }

    private static void onGotManifestData(WebApkUpdateManager updateManager,
            ManifestData fetchedManifestData) {
        String bestIconUrl = randomIconUrl(fetchedManifestData);
        updateManager.onGotManifestData(infoFromManifestData(fetchedManifestData), bestIconUrl);
    }

    private static String randomIconUrl(ManifestData fetchedManifestData) {
        if (fetchedManifestData == null || fetchedManifestData.iconUrlToMurmur2HashMap.isEmpty()) {
            return null;
        }
        return fetchedManifestData.iconUrlToMurmur2HashMap.keySet().iterator().next();
    }

    /**
     * Runs {@link WebApkUpdateManager#updateIfNeeded()} and returns whether an
     * is-update-needed check has been triggered.
     */
    private boolean updateIfNeededChecksForUpdatedWebManifest() {
        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock, getStorage());
        updateIfNeeded(updateManager);
        return updateManager.updateCheckStarted();
    }

    /**
     * Checks whether the WebAPK is updated given data from the WebAPK's Android Manifest and data
     * from the fetched Web Manifest.
     */
    private boolean checkUpdateNeededForFetchedManifest(
            ManifestData androidManifestData, ManifestData fetchedManifestData) {
        registerWebApk(androidManifestData, WebApkVersion.CURRENT_SHELL_APK_VERSION);
        mClock.advance(WebApkUpdateManager.FULL_CHECK_UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock, getStorage());
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());
        updateManager.onGotManifestData(
                infoFromManifestData(fetchedManifestData), fetchedManifestData.bestIconUrl);
        return updateManager.updateRequested();
    }

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.application);
        CommandLine.init(null);
        ChromeWebApkHost.initForTesting(true);

        registerWebApk(defaultManifestData(), WebApkVersion.CURRENT_SHELL_APK_VERSION);
        Settings.Secure.putInt(RuntimeEnvironment.application.getContentResolver(),
                Settings.Secure.INSTALL_NON_MARKET_APPS, 1);

        mClock = new MockClock();
        WebappDataStorage.setClockForTests(mClock);

        WebappRegistry.getInstance().register(
                WEBAPK_ID, new WebappRegistry.FetchWebappDataStorageCallback() {
                    @Override
                    public void onWebappDataStorageRetrieved(WebappDataStorage storage) {}
                });
        ShadowApplication.getInstance().runBackgroundTasks();

        WebappDataStorage storage = getStorage();
        storage.updateTimeOfLastCheckForUpdatedWebManifest();
        storage.updateTimeOfLastWebApkUpdateRequestCompletion();
        storage.updateDidLastWebApkUpdateRequestSucceed(true);
    }

    /**
     * Test that if the WebAPK update failed (e.g. because the WebAPK server is not reachable) that
     * the is-update-needed check is retried after less time than if the WebAPK update had
     * succeeded.
     * The is-update-needed check is the first step in retrying to update the WebAPK.
     */
    @Test
    public void testCheckUpdateMoreFrequentlyIfUpdateFails() {
        assertTrue(WebApkUpdateManager.FULL_CHECK_UPDATE_INTERVAL
                > WebApkUpdateManager.RETRY_UPDATE_DURATION);

        WebappDataStorage storage = getStorage();

        assertTrue(storage.getDidLastWebApkUpdateRequestSucceed());
        assertFalse(updateIfNeededChecksForUpdatedWebManifest());
        mClock.advance(WebApkUpdateManager.RETRY_UPDATE_DURATION);
        assertFalse(updateIfNeededChecksForUpdatedWebManifest());

        // Advance all of the time stamps.
        storage.updateTimeOfLastCheckForUpdatedWebManifest();
        storage.updateTimeOfLastWebApkUpdateRequestCompletion();

        storage.updateDidLastWebApkUpdateRequestSucceed(false);
        assertFalse(updateIfNeededChecksForUpdatedWebManifest());
        mClock.advance(WebApkUpdateManager.RETRY_UPDATE_DURATION);
        assertTrue(updateIfNeededChecksForUpdatedWebManifest());
    }

    /**
     * Test that if there was no previous WebAPK update attempt that the is-update-needed check is
     * done after the usual delay (as opposed to the shorter delay if the previous WebAPK update
     * failed.)
     */
    @Test
    public void testRegularCheckIntervalIfNoPriorWebApkUpdate() {
        assertTrue(WebApkUpdateManager.FULL_CHECK_UPDATE_INTERVAL
                > WebApkUpdateManager.RETRY_UPDATE_DURATION);

        getStorage().delete();
        WebappDataStorage storage = getStorage();

        // Done when WebAPK is registered in {@link WebApkActivity}.
        storage.updateTimeOfLastCheckForUpdatedWebManifest();

        assertFalse(updateIfNeededChecksForUpdatedWebManifest());
        mClock.advance(WebApkUpdateManager.RETRY_UPDATE_DURATION);
        assertFalse(updateIfNeededChecksForUpdatedWebManifest());
        mClock.advance(WebApkUpdateManager.FULL_CHECK_UPDATE_INTERVAL
                - WebApkUpdateManager.RETRY_UPDATE_DURATION);
        assertTrue(updateIfNeededChecksForUpdatedWebManifest());
    }

    /**
     * Test that the is-update-needed check is tried the next time that the WebAPK is launched if
     * Chrome is killed prior to the initial URL finishing loading.
     */
    @Test
    public void testCheckOnNextLaunchIfClosePriorToFirstPageLoad() {
        mClock.advance(WebApkUpdateManager.FULL_CHECK_UPDATE_INTERVAL);
        {
            TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock,
                    getStorage());
            updateIfNeeded(updateManager);
            assertTrue(updateManager.updateCheckStarted());
        }

        // Chrome is killed. Neither
        // {@link WebApkUpdateManager#onWebManifestForInitialUrlNotWebApkCompatible()} nor
        // {@link WebApkUpdateManager#OnGotManifestData()} is called.

        {
            // Relaunching the WebAPK should do an is-update-needed check.
            TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock,
                    getStorage());
            updateIfNeeded(updateManager);
            assertTrue(updateManager.updateCheckStarted());
            onGotUnchangedWebManifestData(updateManager);
        }

        {
            // Relaunching the WebAPK should not do an is-update-needed-check.
            TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock,
                    getStorage());
            updateIfNeeded(updateManager);
            assertFalse(updateManager.updateCheckStarted());
        }
    }

    /**
     * Test that the completion time of the previous WebAPK update is not modified if:
     * - The previous WebAPK update succeeded.
     * AND
     * - A WebAPK update is not required.
     */
    @Test
    public void testUpdateNotNeeded() {
        long initialTime = mClock.currentTimeMillis();
        mClock.advance(WebApkUpdateManager.FULL_CHECK_UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock,
                getStorage());
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotUnchangedWebManifestData(updateManager);
        assertFalse(updateManager.updateRequested());

        WebappDataStorage storage = getStorage();
        assertTrue(storage.getDidLastWebApkUpdateRequestSucceed());
        assertEquals(initialTime, storage.getLastWebApkUpdateRequestCompletionTime());
    }

    /**
     * Test that the last WebAPK update is marked as having succeeded if:
     * - The previous WebAPK update failed.
     * AND
     * - A WebAPK update is no longer required.
     */
    @Test
    public void testMarkUpdateAsSucceededIfUpdateNoLongerNeeded() {
        WebappDataStorage storage = getStorage();
        storage.updateDidLastWebApkUpdateRequestSucceed(false);
        mClock.advance(WebApkUpdateManager.RETRY_UPDATE_DURATION);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock, getStorage());
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotUnchangedWebManifestData(updateManager);
        assertFalse(updateManager.updateRequested());

        assertTrue(storage.getDidLastWebApkUpdateRequestSucceed());
        assertEquals(
                mClock.currentTimeMillis(), storage.getLastWebApkUpdateRequestCompletionTime());
    }

    /**
     * Test that the WebAPK update is marked as having failed if Chrome is killed prior to the
     * WebAPK update completing.
     */
    @Test
    public void testMarkUpdateAsFailedIfClosePriorToUpdateCompleting() {
        mClock.advance(WebApkUpdateManager.FULL_CHECK_UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock, getStorage());
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());
        ManifestData manifestData = defaultManifestData();
        manifestData.name = DIFFERENT_NAME;
        onGotManifestData(updateManager, manifestData);
        assertTrue(updateManager.updateRequested());

        // Chrome is killed. {@link WebApkUpdateManager#onBuiltWebApk} is never called.

        // Check {@link WebappDataStorage} state.
        WebappDataStorage storage = getStorage();
        assertFalse(storage.getDidLastWebApkUpdateRequestSucceed());
        assertEquals(
                mClock.currentTimeMillis(), storage.getLastWebApkUpdateRequestCompletionTime());
    }

    /**
     * Test that an update with data from the WebAPK's Android manifest is done if:
     * - WebAPK's code is out of date
     * AND
     * - WebAPK's start_url does not refer to a Web Manifest.
     *
     * It is good to minimize the number of users with out of date WebAPKs. We try to keep WebAPKs
     * up to date even if the web developer has removed the Web Manifest from their site.
     */
    @Test
    public void testShellApkOutOfDateNoWebManifest() {
        registerWebApk(defaultManifestData(), WebApkVersion.CURRENT_SHELL_APK_VERSION - 1);
        mClock.advance(WebApkUpdateManager.FULL_CHECK_UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock, getStorage());
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());

        updateManager.onWebManifestForInitialUrlNotWebApkCompatible();
        assertTrue(updateManager.updateRequested());
        assertEquals(NAME, updateManager.requestedUpdateName());

        // Check that the {@link ManifestUpgradeDetector} has been destroyed. This prevents
        // {@link #onWebManifestForInitialUrlNotWebApkCompatible()} and {@link #onGotManifestData()}
        // from getting called.
        assertTrue(updateManager.destroyedFetcher());
    }

    /**
     * Test that an update with data from the fetched Web Manifest is done if the WebAPK's code is
     * out of date and the WebAPK's start_url refers to a Web Manifest.
     */
    @Test
    public void testShellApkOutOfDateStillHasWebManifest() {
        registerWebApk(defaultManifestData(), WebApkVersion.CURRENT_SHELL_APK_VERSION - 1);
        mClock.advance(WebApkUpdateManager.FULL_CHECK_UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock, getStorage());
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());

        onGotManifestData(updateManager, defaultManifestData());
        assertTrue(updateManager.updateRequested());
        assertEquals(NAME, updateManager.requestedUpdateName());

        assertTrue(updateManager.destroyedFetcher());
    }

    /**
     * Test that an update is requested if:
     * - start_url does not refer to a Web Manifest.
     * AND
     * - The user eventually navigates to a page pointing to a Web Manifest with the correct URL.
     * AND
     * - The Web Manifest has changed.
     *
     * This scenario can occur if the WebAPK's start_url is a Javascript redirect.
     */
    @Test
    public void testStartUrlRedirectsToPageWithUpdatedWebManifest() {
        mClock.advance(WebApkUpdateManager.FULL_CHECK_UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock, getStorage());
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());

        // start_url does not have a Web Manifest. No update should be requested.
        updateManager.onWebManifestForInitialUrlNotWebApkCompatible();
        assertFalse(updateManager.updateRequested());
        // {@link ManifestUpgradeDetector} should still be alive so that it can get
        // {@link #onGotManifestData} when page with the Web Manifest finishes loading.
        assertFalse(updateManager.destroyedFetcher());

        // start_url redirects to page with Web Manifest.

        ManifestData manifestData = defaultManifestData();
        manifestData.name = DIFFERENT_NAME;
        onGotManifestData(updateManager, manifestData);
        assertTrue(updateManager.updateRequested());
        assertEquals(DIFFERENT_NAME, updateManager.requestedUpdateName());

        assertTrue(updateManager.destroyedFetcher());
    }

    /**
     * Test that an update is not requested if:
     * - start_url does not refer to a Web Manifest.
     * AND
     * - The user eventually navigates to a page pointing to a Web Manifest with the correct URL.
     * AND
     * - The Web Manifest has not changed.
     */
    @Test
    public void testStartUrlRedirectsToPageWithUnchangedWebManifest() {
        mClock.advance(WebApkUpdateManager.FULL_CHECK_UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock, getStorage());
        updateIfNeeded(updateManager);
        updateManager.onWebManifestForInitialUrlNotWebApkCompatible();
        onGotManifestData(updateManager, defaultManifestData());
        assertFalse(updateManager.updateRequested());

        // We got the Web Manifest. The {@link ManifestUpgradeDetector} should be destroyed to stop
        // it from fetching the Web Manifest for subsequent page loads.
        assertTrue(updateManager.destroyedFetcher());
    }

    @Test
    public void testManifestDoesNotUpgrade() {
        assertFalse(
                checkUpdateNeededForFetchedManifest(defaultManifestData(), defaultManifestData()));
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
        assertTrue(!oldData.scopeUrl.equals(fetchedData.scopeUrl));
        assertFalse(checkUpdateNeededForFetchedManifest(oldData, fetchedData));
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
        assertTrue(
                !oldData.scopeUrl.equals(ShortcutHelper.getScopeFromUrl(oldData.startUrl)));
        ManifestData fetchedData = defaultManifestData();
        fetchedData.startUrl = "/fancy/scope/special/snowflake.html";
        fetchedData.scopeUrl = "";

        assertTrue(checkUpdateNeededForFetchedManifest(oldData, fetchedData));
    }

    /**
     * Test that an upgrade is requested when:
     * - WebAPK was generated using icon at {@link ICON_URL} from Web Manifest.
     * - Bitmap at {@link ICON_URL} has changed.
     */
    @Test
    public void testHomescreenIconChangeShouldUpgrade() {
        ManifestData fetchedData = defaultManifestData();
        fetchedData.iconUrlToMurmur2HashMap.put(fetchedData.bestIconUrl, ICON_MURMUR2_HASH + "1");
        fetchedData.bestIcon = createBitmap(Color.BLUE);
        assertTrue(checkUpdateNeededForFetchedManifest(defaultManifestData(), fetchedData));
    }

    /**
     * Test that an upgrade is requested when:
     * - WebAPK is generated using icon at {@link ICON_URL} from Web Manifest.
     * - Web Manifest is updated to refer to different icon.
     */
    @Test
    public void testHomescreenBestIconUrlChangeShouldUpgrade() {
        ManifestData fetchedData = defaultManifestData();
        fetchedData.iconUrlToMurmur2HashMap.clear();
        fetchedData.iconUrlToMurmur2HashMap.put("/icon2.png", "22");
        fetchedData.bestIconUrl = "/icon2.png";
        assertTrue(checkUpdateNeededForFetchedManifest(defaultManifestData(), fetchedData));
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
        fetchedData.iconUrlToMurmur2HashMap.put(ICON_URL, ICON_MURMUR2_HASH);
        fetchedData.iconUrlToMurmur2HashMap.put("/icon2.png", null);
        assertFalse(checkUpdateNeededForFetchedManifest(defaultManifestData(), fetchedData));
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

        assertFalse(checkUpdateNeededForFetchedManifest(oldData, fetchedData));
    }

    @Test
    public void testForceUpdateWhenUncompletedUpdateRequestRechesMaximumTimes() {
        mClock.advance(WebApkUpdateManager.FULL_CHECK_UPDATE_INTERVAL);
        ManifestData differentManifestData = defaultManifestData();
        differentManifestData.name = DIFFERENT_NAME;
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(WEBAPK_ID);

        for (int i = 0; i < 3; ++i) {
            TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock,
                    getStorage());
            updateManager.setIsWebApkForeground(true);
            updateIfNeeded(updateManager);

            onGotManifestData(updateManager, differentManifestData);
            assertTrue(updateManager.getHasPendingUpdateForTesting());
            assertFalse(updateManager.updateRequested());
            assertEquals(i + 1, storage.getUpdateRequests());
        }

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock, getStorage());
        updateManager.setIsWebApkForeground(true);
        updateIfNeeded(updateManager);

        onGotManifestData(updateManager, differentManifestData);
        assertFalse(updateManager.getHasPendingUpdateForTesting());
        assertTrue(updateManager.updateRequested());
        assertEquals(0, storage.getUpdateRequests());
    }

    @Test
    public void testRequestUpdateAfterWebApkOnStopIsCalled() {
        ManifestData differentManifestData = defaultManifestData();
        differentManifestData.name = DIFFERENT_NAME;
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(WEBAPK_ID);

        mClock.advance(WebApkUpdateManager.FULL_CHECK_UPDATE_INTERVAL);
        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock, getStorage());
        updateManager.setIsWebApkForeground(true);
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());

        onGotManifestData(updateManager, differentManifestData);
        assertTrue(updateManager.getHasPendingUpdateForTesting());
        assertFalse(updateManager.updateRequested());
        assertEquals(1, storage.getUpdateRequests());

        // Since {@link WebApkActivity#OnStop()} calls {@link requestPendingUpdate()} to trigger an
        // update request, we call it directly for testing.
        updateManager.setIsWebApkForeground(false);
        updateManager.requestPendingUpdate();

        assertFalse(updateManager.getHasPendingUpdateForTesting());
        assertTrue(updateManager.updateRequested());
        assertEquals(0, storage.getUpdateRequests());
    }
}
