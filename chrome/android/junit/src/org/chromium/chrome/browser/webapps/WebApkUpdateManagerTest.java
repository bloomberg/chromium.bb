// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.os.Bundle;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

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
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

/**
 * Unit tests for WebApkUpdateManager.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebApkUpdateManagerTest {
    /** WebAPK's id in {@link WebAppDataStorage}. */
    private static final String WEBAPK_ID = "id";

    /** WebAPK's start URL. */
    private static final String WEBAPK_START_URL = "https://www.unicode.party";

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

        public TestManifestUpgradeDetector(Tab tab, WebappInfo info, Bundle metaData,
                ManifestUpgradeDetector.Callback callback) {
            super(tab, info, metaData, callback);
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
        private boolean mUpdateRequested;

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
            return mUpdateRequested;
        }

        @Override
        protected ManifestUpgradeDetector buildManifestUpgradeDetector(
                Tab tab, WebappInfo info, Bundle metaData) {
            mUpgradeDetector = new TestManifestUpgradeDetector(tab, info, metaData, this);
            return mUpgradeDetector;
        }

        @Override
        public void updateAsync(
                String manifestUrl, ManifestUpgradeDetector.FetchedManifestData data) {
            mUpdateRequested = true;
        }

        @Override
        protected long currentTimeMillis() {
            return mClock.currentTimeMillis();
        }
    }

    private MockClock mClock;

    private WebappDataStorage getStorage() {
        return WebappRegistry.getInstance().getWebappDataStorage(WEBAPK_ID);
    }

    private void updateIfNeeded(WebApkUpdateManager updateManager) {
        WebappInfo info = WebappInfo.create(WEBAPK_ID, WEBAPK_START_URL, null, null, null, null,
                WebDisplayMode.Standalone, ScreenOrientationValues.DEFAULT, ShortcutSource.UNKNOWN,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING, false,
                WebApkTestHelper.WEBAPK_PACKAGE_NAME);
        updateManager.updateIfNeeded(null, info);
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

        mClock = new MockClock();
        WebappDataStorage.setClockForTests(mClock);

        Bundle metaData = new Bundle();
        metaData.putInt(
                WebApkMetaDataKeys.SHELL_APK_VERSION, WebApkVersion.CURRENT_SHELL_APK_VERSION);
        WebApkTestHelper.registerWebApkWithMetaData(metaData);

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
        updateManager.onUpgradeNeededCheckFinished(false, null);
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
        updateManager.onUpgradeNeededCheckFinished(false, null);
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
        updateManager.onUpgradeNeededCheckFinished(true, null);
        assertTrue(updateManager.updateRequested());

        // Chrome is killed. {@link WebApkUpdateManager#onBuiltWebApk} is never called.

        // Check {@link WebappDataStorage} state.
        WebappDataStorage storage = getStorage();
        assertFalse(storage.getDidLastWebApkUpdateRequestSucceed());
        assertEquals(
                mClock.currentTimeMillis(), storage.getLastWebApkUpdateRequestCompletionTime());
    }
}
