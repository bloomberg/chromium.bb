// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.precache;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.Context;
import android.support.test.filters.SmallTest;

import com.google.android.gms.gcm.Task;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.content.browser.test.NativeLibraryTestBase;

import java.util.EnumSet;
import java.util.concurrent.Callable;

/**
 * Unit tests for {@link PrecacheLauncher}.
 *
 * setUp/tearDown code was inspired by org.chromium.chrome.browser.sync.ui.PassphraseActivityTest.
 */
public class PrecacheLauncherTest extends NativeLibraryTestBase {
    private StubProfileSyncService mSync;
    private PrecacheLauncherUnderTest mLauncher;
    private MockPrecacheTaskScheduler mPrecacheTaskScheduler;

    private class PrecacheLauncherUnderTest extends PrecacheLauncher {
        private boolean mShouldRun = false;

        @Override
        protected void onPrecacheCompleted(boolean tryAgainSoon) {}

        @Override
        boolean nativeShouldRun() {
            return mShouldRun;
        }

        /**
         * Modify the return value of nativeShouldRun. This will notify sync state subscribers, as
         * if the user changed their sync preferences.
         */
        void setShouldRun(boolean shouldRun) {
            mShouldRun = shouldRun;
            notifySyncChanged();
        }
    }

    private static class StubProfileSyncService extends ProfileSyncService {
        private boolean mEngineInitialized = false;

        public StubProfileSyncService() {
            super();
        }

        @Override
        public boolean isEngineInitialized() {
            return mEngineInitialized;
        }

        public void setEngineInitialized(boolean engineInitialized) {
            mEngineInitialized = engineInitialized;
            syncStateChanged();
        }
    }

    static class MockPrecacheTaskScheduler extends PrecacheTaskScheduler {
        @Override
        boolean scheduleTask(Context context, Task task) {
            return true;
        }

        @Override
        boolean cancelTask(Context context, String tag) {
            return true;
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        ContextUtils.initApplicationContext(getTargetContext().getApplicationContext());

        // This is a PrecacheLauncher with a stubbed out nativeShouldRun so we can change that on
        // the fly without needing to set up a sync engine.
        mLauncher = new PrecacheLauncherUnderTest();

        mPrecacheTaskScheduler = new MockPrecacheTaskScheduler();
        PrecacheController.setTaskScheduler(mPrecacheTaskScheduler);

        // The target context persists throughout the entire test run, and so leaks state between
        // tests. We reset the is_precaching_enabled pref to false to make the test run consistent,
        // in case another test class has modified this pref.
        PrecacheController.setIsPrecachingEnabled(getTargetContext(), false);

        // ProfileSyncService needs the browser process to be running.
        loadNativeLibraryAndInitBrowserProcess();

        // ProfileSyncService must be initialized on the UI thread. Oddly, even though
        // ThreadUtils.runningOnUiThread() is true here, it's, no, not really, no.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // The StubProfileSyncService stubs out isEngineInitialized so we can change that
                // on the fly.
                mSync = new StubProfileSyncService();
                ProfileSyncService.overrideForTests(mSync);
                // This is currently the default, but let's verify that, lest it ever change and we
                // get confusing test failures later.
                assertTrue(PrivacyPreferencesManager.getInstance().shouldPrerender());
            }
        });
    }

    @Override
    protected void tearDown() throws Exception {
        ProfileSyncService.overrideForTests(null);
        PrecacheController.setIsPrecachingEnabled(getTargetContext(), false);
        super.tearDown();
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Precache"})
    public void testUpdateEnabled_SyncNotReady_ThenDisabled() {
        mLauncher.updateEnabled(getTargetContext());
        waitUntilUiThreadIdle();

        assertEquals(false, isPrecachingEnabled());
        assertEquals(EnumSet.of(FailureReason.SYNC_NOT_INITIALIZED,
                             FailureReason.PRERENDER_PRIVACY_PREFERENCE_NOT_ENABLED,
                             FailureReason.NATIVE_SHOULD_RUN_IS_FALSE),
                failureReasons());

        setEngineInitialized(true);
        assertEquals(false, isPrecachingEnabled());
        assertEquals(EnumSet.of(FailureReason.NATIVE_SHOULD_RUN_IS_FALSE), failureReasons());
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Precache"})
    public void testUpdateEnabled_SyncNotReady_ThenEnabled() {
        mLauncher.updateEnabled(getTargetContext());
        waitUntilUiThreadIdle();

        assertEquals(false, isPrecachingEnabled());
        assertEquals(EnumSet.of(FailureReason.SYNC_NOT_INITIALIZED,
                             FailureReason.PRERENDER_PRIVACY_PREFERENCE_NOT_ENABLED,
                             FailureReason.NATIVE_SHOULD_RUN_IS_FALSE),
                failureReasons());

        mLauncher.setShouldRun(true);
        setEngineInitialized(true);
        assertEquals(true, isPrecachingEnabled());
        assertEquals(EnumSet.noneOf(FailureReason.class), failureReasons());
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Precache"})
    public void testUpdateEnabled_Disabled_ThenEnabled() {
        setEngineInitialized(true);
        mLauncher.updateEnabled(getTargetContext());
        waitUntilUiThreadIdle();

        assertEquals(false, isPrecachingEnabled());
        assertEquals(EnumSet.of(FailureReason.NATIVE_SHOULD_RUN_IS_FALSE), failureReasons());

        mLauncher.setShouldRun(true);
        assertEquals(true, isPrecachingEnabled());
        assertEquals(EnumSet.noneOf(FailureReason.class), failureReasons());
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Precache"})
    public void testUpdateEnabled_Enabled_ThenDisabled() {
        mLauncher.setShouldRun(true);
        setEngineInitialized(true);
        mLauncher.updateEnabled(getTargetContext());
        waitUntilUiThreadIdle();

        assertEquals(true, isPrecachingEnabled());
        assertEquals(EnumSet.noneOf(FailureReason.class), failureReasons());

        mLauncher.setShouldRun(false);
        assertEquals(false, isPrecachingEnabled());
        assertEquals(EnumSet.of(FailureReason.NATIVE_SHOULD_RUN_IS_FALSE), failureReasons());
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Precache"})
    public void testUpdateEnabledNullProfileSyncService() {
        ProfileSyncService.overrideForTests(null);

        mLauncher.updateEnabled(getTargetContext());
        waitUntilUiThreadIdle();

        assertEquals(false, isPrecachingEnabled());
        assertEquals(EnumSet.of(FailureReason.SYNC_NOT_INITIALIZED,
                             FailureReason.PRERENDER_PRIVACY_PREFERENCE_NOT_ENABLED,
                             FailureReason.NATIVE_SHOULD_RUN_IS_FALSE),
                failureReasons());
    }

    /** Return the Context for the Chromium app. */
    private Context getTargetContext() {
        return getInstrumentation().getTargetContext();
    }

    /** Block until all tasks posted to the UI thread have completed. */
    private void waitUntilUiThreadIdle() {
        getInstrumentation().waitForIdleSync();
    }

    /** Return the value of the is_precaching_enabled pref, as set by updateEnabledSync. */
    private boolean isPrecachingEnabled() {
        return PrecacheController.get(getTargetContext()).isPrecachingEnabled();
    }

    /** Return the set of failure reasons for mLauncher. */
    private EnumSet<FailureReason> failureReasons() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<EnumSet<FailureReason>>() {
            @Override
            public EnumSet<FailureReason> call() {
                return mLauncher.failureReasons();
            }
        });
    }

    /** Pretend the sync engine is initialized or not. */
    private void setEngineInitialized(final boolean syncInitialized) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSync.setEngineInitialized(syncInitialized);
            }
        });
    }

    /** Notify listeners that sync preferences have changed. This is run by setShouldRun. */
    private void notifySyncChanged() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSync.syncStateChanged();
            }
        });
    }
}
