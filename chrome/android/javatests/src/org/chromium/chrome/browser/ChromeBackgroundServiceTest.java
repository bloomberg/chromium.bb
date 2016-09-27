// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import com.google.android.gms.gcm.TaskParams;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ntp.snippets.SnippetsLauncher;
import org.chromium.chrome.browser.precache.PrecacheController;

/**
 * Tests {@link ChromeBackgroundService}.
 */
@RetryOnFailure
public class ChromeBackgroundServiceTest extends InstrumentationTestCase {
    private Context mContext;
    private BackgroundSyncLauncher mSyncLauncher;
    private SnippetsLauncher mSnippetsLauncher;
    private MockTaskService mTaskService;

    static class MockTaskService extends ChromeBackgroundService {
        private boolean mDidLaunchBrowser = false;
        private boolean mDidFetchSnippets = false;
        private boolean mDidRescheduleFetching = false;
        private boolean mHasPrecacheInstance = true;
        private boolean mPrecachingStarted = false;

        @Override
        protected void launchBrowser(Context context, String tag) {
            mDidLaunchBrowser = true;
        }

        @Override
        protected void fetchSnippets() {
            mDidFetchSnippets = true;
        }

        @Override
        protected void rescheduleFetching() {
            mDidRescheduleFetching = true;
        }

        @Override
        protected boolean hasPrecacheInstance() {
            return mHasPrecacheInstance;
        }

        @Override
        protected void precache(Context context, String tag) {
            if (!mHasPrecacheInstance) {
                mPrecachingStarted = true;
            }
        }

        @Override
        protected void rescheduleBackgroundSyncTasksOnUpgrade() {}

        @Override
        protected void reschedulePrecacheTasksOnUpgrade() {}

        // Posts an assertion task to the UI thread. Since this is only called after the call
        // to onRunTask, it will be enqueued after any possible call to launchBrowser, and we
        // can reliably check whether launchBrowser was called.
        protected void checkExpectations(final boolean expectedLaunchBrowser,
                final boolean expectedPrecacheStarted, final boolean expectedFetchSnippets,
                final boolean expectedRescheduleFetching) {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    assertEquals("StartedService", expectedLaunchBrowser, mDidLaunchBrowser);
                    assertEquals("StartedPrecache", expectedPrecacheStarted, mPrecachingStarted);
                    assertEquals("FetchedSnippets", expectedFetchSnippets, mDidFetchSnippets);
                    assertEquals("RescheduledFetching", expectedRescheduleFetching,
                            mDidRescheduleFetching);
                }
            });
        }

        protected void deletePrecacheInstance() {
            mHasPrecacheInstance = false;
        }
    }

    @Override
    protected void setUp() throws Exception {
        mContext = new AdvancedMockContext(getInstrumentation().getTargetContext());
        BackgroundSyncLauncher.setGCMEnabled(false);
        RecordHistogram.disableForTests();
        mSyncLauncher = BackgroundSyncLauncher.create(mContext);
        mSnippetsLauncher = SnippetsLauncher.create(mContext);
        mTaskService = new MockTaskService();
    }

    private void deleteSyncLauncherInstance() {
        mSyncLauncher.destroy();
        mSyncLauncher = null;
    }

    private void deleteSnippetsLauncherInstance() {
        mSnippetsLauncher.destroy();
        mSnippetsLauncher = null;
    }

    private void startOnRunTaskAndVerify(String taskTag, boolean shouldStart,
            boolean shouldPrecache, boolean shouldFetchSnippets) {
        mTaskService.onRunTask(new TaskParams(taskTag));
        mTaskService.checkExpectations(shouldStart, shouldPrecache, shouldFetchSnippets, false);
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testBackgroundSyncNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(BackgroundSyncLauncher.TASK_TAG, false, false, false);
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testBackgroundSyncLaunchBrowserWhenInstanceDoesNotExist() {
        deleteSyncLauncherInstance();
        startOnRunTaskAndVerify(BackgroundSyncLauncher.TASK_TAG, true, false, false);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchWifiNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_WIFI, false, false, true);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchFallbackNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_FALLBACK, false, false, true);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchWifiLaunchBrowserWhenInstanceDoesNotExist() {
        deleteSnippetsLauncherInstance();
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_WIFI, true, false, true);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchFallbackLaunchBrowserWhenInstanceDoesNotExist() {
        deleteSnippetsLauncherInstance();
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_FALLBACK, true, false, true);
    }

    @SmallTest
    @Feature({"Precache"})
    public void testPrecacheNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(PrecacheController.PERIODIC_TASK_TAG, false, false, false);
    }

    @SmallTest
    @Feature({"Precache"})
    public void testPrecacheLaunchBrowserWhenInstanceDoesNotExist() {
        mTaskService.deletePrecacheInstance();
        startOnRunTaskAndVerify(PrecacheController.PERIODIC_TASK_TAG, true, true, false);
    }

    private void startOnInitializeTasksAndVerify(boolean shouldStart, boolean shouldReschedule) {
        mTaskService.onInitializeTasks();
        mTaskService.checkExpectations(shouldStart, false, false, shouldReschedule);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsNoRescheduleWithoutPrefWhenInstanceExists() {
        startOnInitializeTasksAndVerify(/*shouldStart=*/false, /*shouldReschedule=*/false);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsNoRescheduleWithoutPrefWhenInstanceDoesNotExist() {
        deleteSnippetsLauncherInstance();
        startOnInitializeTasksAndVerify(/*shouldStart=*/false, /*shouldReschedule=*/false);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsRescheduleWithPrefWhenInstanceExists() {
        // Set the pref indicating that fetching was scheduled before.
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(SnippetsLauncher.PREF_IS_SCHEDULED, true)
                .apply();

        startOnInitializeTasksAndVerify(/*shouldStart=*/false, /*shouldReschedule=*/true);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsRescheduleAndLaunchBrowserWithPrefWhenInstanceDoesNotExist() {
        deleteSnippetsLauncherInstance();
        // Set the pref indicating that fetching was scheduled before.
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(SnippetsLauncher.PREF_IS_SCHEDULED, true)
                .apply();

        startOnInitializeTasksAndVerify(/*shouldStart=*/true, /*shouldReschedule=*/true);
    }
}
