// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.provider.Settings;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.webapk.lib.client.WebApkVersion;
import org.chromium.webapk.test.WebApkTestHelper;

import java.util.HashMap;

/**
 * Unit tests for WebApkUpdateManager.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebApkUpdateManagerTest {
    /** WebAPK's id in {@link WebAppDataStorage}. */
    private static final String WEBAPK_ID = "id";

    /** Value of the "name" <meta-data> tag in the Android Manifest. */
    private static final String ANDROID_MANIFEST_NAME = "Android Manifest name";

    /** Value of the "name" property in the Web Manifest. */
    private static final String WEB_MANIFEST_NAME = "Web Manifest name";

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

    /** Mock {@link ManifestUpgradeDetector}. */
    private static class TestManifestUpgradeDetector extends ManifestUpgradeDetector {
        private boolean mStarted;

        public TestManifestUpgradeDetector(
                Tab tab, WebApkInfo info, ManifestUpgradeDetector.Callback callback) {
            super(tab, info, callback);
        }

        public boolean wasStarted() {
            return mStarted;
        }

        @Override
        public boolean start() {
            mStarted = true;
            return true;
        }
    }

    private static class TestWebApkUpdateManager extends WebApkUpdateManager {
        private WebappDataStorage.Clock mClock;
        private TestManifestUpgradeDetector mUpgradeDetector;
        private int mNumUpdatesRequested;
        private String mUpdateName;
        private boolean mDestroyedManifestUpgradeDetector;

        public TestWebApkUpdateManager(WebappDataStorage.Clock clock) {
            mClock = clock;
        }

        /**
         * Returns whether the is-update-needed check has been triggered.
         */
        public boolean updateCheckStarted() {
            return mUpgradeDetector != null && mUpgradeDetector.wasStarted();
        }

        /**
         * Returns whether an update has been requested.
         */
        public boolean updateRequested() {
            return mNumUpdatesRequested > 0;
        }

        /**
         * Returns the number of updates which have been requested.
         */
        public int numUpdatesRequested() {
            return mNumUpdatesRequested;
        }

        /**
         * Returns the "name" from the requested update. Null if an update has not been requested.
         */
        public String requestedUpdateName() {
            return mUpdateName;
        }

        public boolean destroyedManifestUpgradeDetector() {
            return mDestroyedManifestUpgradeDetector;
        }

        @Override
        protected ManifestUpgradeDetector buildManifestUpgradeDetector(Tab tab, WebApkInfo info) {
            mUpgradeDetector = new TestManifestUpgradeDetector(tab, info, this);
            return mUpgradeDetector;
        }

        @Override
        protected void updateAsync(WebApkInfo info, String bestIconUrl) {
            ++mNumUpdatesRequested;
            mUpdateName = info.name();
        }

        @Override
        protected void destroyUpgradeDetector() {
            mUpgradeDetector = null;
            mDestroyedManifestUpgradeDetector = true;
        }

        @Override
        protected long currentTimeMillis() {
            return mClock.currentTimeMillis();
        }
    }

    private MockClock mClock;
    private int mShellApkVersion;

    /** Sets the version of the code in //chrome/android/webapk/shell_apk. */
    public void setShellApkVersion(int shellApkVersion) {
        mShellApkVersion = shellApkVersion;
    }

    private WebappDataStorage getStorage() {
        return WebappRegistry.getInstance().getWebappDataStorage(WEBAPK_ID);
    }

    private WebApkInfo infoWithName(String name) {
        return WebApkInfo.create(WEBAPK_ID, "", "", null, name, "", WebDisplayMode.Standalone,
                ScreenOrientationValues.DEFAULT, ShortcutSource.UNKNOWN,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                WebApkTestHelper.WEBAPK_PACKAGE_NAME, mShellApkVersion, "", "",
                new HashMap<String, String>());
    }

    private WebApkInfo fetchedWebApkInfo() {
        return infoWithName(WEB_MANIFEST_NAME);
    }

    private void updateIfNeeded(WebApkUpdateManager updateManager) {
        WebApkInfo info = infoWithName(ANDROID_MANIFEST_NAME);
        updateManager.updateIfNeeded(null, info);
    }

    private void onGotWebApkCompatibleWebManifestForInitialUrl(
            WebApkUpdateManager updateManager, boolean needsUpdate) {
        updateManager.onFinishedFetchingWebManifestForInitialUrl(
                needsUpdate, fetchedWebApkInfo(), null);
    }

    /**
     * Runs {@link WebApkUpdateManager#updateIfNeeded()} and returns whether an
     * is-update-needed check has been triggered.
     */
    private boolean updateIfNeededChecksForUpdatedWebManifest() {
        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock);
        updateIfNeeded(updateManager);
        return updateManager.updateCheckStarted();
    }

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.application);
        CommandLine.init(null);

        Settings.Secure.putInt(RuntimeEnvironment.application.getContentResolver(),
                Settings.Secure.INSTALL_NON_MARKET_APPS, 1);

        mClock = new MockClock();
        WebappDataStorage.setClockForTests(mClock);

        mShellApkVersion = WebApkVersion.CURRENT_SHELL_APK_VERSION;

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
            TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock);
            updateIfNeeded(updateManager);
            assertTrue(updateManager.updateCheckStarted());
        }

        // Chrome is killed.
        // {@link WebApkUpdateManager#onFinishedFetchingWebManifestForInitialUrl()} is never called.

        {
            // Relaunching the WebAPK should do an is-update-needed check.
            TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock);
            updateIfNeeded(updateManager);
            assertTrue(updateManager.updateCheckStarted());
            onGotWebApkCompatibleWebManifestForInitialUrl(updateManager, false);
        }

        {
            // Relaunching the WebAPK should not do an is-update-needed-check.
            TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock);
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

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock);
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotWebApkCompatibleWebManifestForInitialUrl(updateManager, false);
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

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock);
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotWebApkCompatibleWebManifestForInitialUrl(updateManager, false);
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

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock);
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotWebApkCompatibleWebManifestForInitialUrl(updateManager, true);
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
        setShellApkVersion(WebApkVersion.CURRENT_SHELL_APK_VERSION - 1);
        mClock.advance(WebApkUpdateManager.FULL_CHECK_UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock);
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());

        updateManager.onFinishedFetchingWebManifestForInitialUrl(false, null, null);
        assertTrue(updateManager.updateRequested());
        assertEquals(ANDROID_MANIFEST_NAME, updateManager.requestedUpdateName());

        // Check that the {@link ManifestUpgradeDetector} has been destroyed. This prevents
        // {@link #onFinishedFetchingWebManifestForInitialUrl()} and {@link #onGotManifestData()}
        // from getting called.
        assertTrue(updateManager.destroyedManifestUpgradeDetector());
    }

    /**
     * Test that an update with data from the fetched Web Manifest is done if the WebAPK's code is
     * out of date and the WebAPK's start_url refers to a Web Manifest.
     */
    @Test
    public void testShellApkOutOfDateStillHasWebManifest() {
        setShellApkVersion(WebApkVersion.CURRENT_SHELL_APK_VERSION - 1);
        mClock.advance(WebApkUpdateManager.FULL_CHECK_UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock);
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());

        updateManager.onFinishedFetchingWebManifestForInitialUrl(false, fetchedWebApkInfo(), null);
        assertTrue(updateManager.updateRequested());
        assertEquals(WEB_MANIFEST_NAME, updateManager.requestedUpdateName());

        assertTrue(updateManager.destroyedManifestUpgradeDetector());
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

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock);
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());

        // start_url does not have a Web Manifest. No update should be requested.
        updateManager.onFinishedFetchingWebManifestForInitialUrl(false, null, null);
        assertFalse(updateManager.updateRequested());
        // {@link ManifestUpgradeDetector} should still be alive so that it can get
        // {@link #onGotManifestData} when page with the Web Manifest finishes loading.
        assertFalse(updateManager.destroyedManifestUpgradeDetector());

        // start_url redirects to page with Web Manifest.

        updateManager.onGotManifestData(true, fetchedWebApkInfo(), null);
        assertTrue(updateManager.updateRequested());
        assertEquals(WEB_MANIFEST_NAME, updateManager.requestedUpdateName());

        assertTrue(updateManager.destroyedManifestUpgradeDetector());
    }

    /**
     * Test than an update is not requested if:
     * - start_url does not refer to a Web Manifest.
     * AND
     * - The user eventually navigates to a page pointing to a Web Manifest with the correct URL.
     * AND
     * - The Web Manifest has not changed.
     */
    @Test
    public void testStartUrlRedirectsToPageWithUnchangedWebManifest() {
        mClock.advance(WebApkUpdateManager.FULL_CHECK_UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mClock);
        updateIfNeeded(updateManager);
        updateManager.onFinishedFetchingWebManifestForInitialUrl(false, null, null);
        updateManager.onGotManifestData(false, fetchedWebApkInfo(), null);
        assertFalse(updateManager.updateRequested());

        // We got the Web Manifest. The {@link ManifestUpgradeDetector} should be destroyed to stop
        // it from fetching the Web Manifest for subsequent page loads.
        assertTrue(updateManager.destroyedManifestUpgradeDetector());
    }
}
