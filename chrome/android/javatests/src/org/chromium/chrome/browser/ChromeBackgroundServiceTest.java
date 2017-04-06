// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.support.test.filters.SmallTest;

import com.google.android.gms.gcm.TaskParams;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ntp.snippets.SnippetsLauncher;
import org.chromium.chrome.browser.precache.PrecacheController;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests {@link ChromeBackgroundService}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
public class ChromeBackgroundServiceTest {
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

        @Override
        protected void rescheduleOfflinePagesTasksOnUpgrade() {}

        // Posts an assertion task to the UI thread. Since this is only called after the call
        // to onRunTask, it will be enqueued after any possible call to launchBrowser, and we
        // can reliably check whether launchBrowser was called.
        protected void checkExpectations(final boolean expectedLaunchBrowser,
                final boolean expectedPrecacheStarted, final boolean expectedFetchSnippets,
                final boolean expectedRescheduleFetching) {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    Assert.assertEquals("StartedService", expectedLaunchBrowser, mDidLaunchBrowser);
                    Assert.assertEquals(
                            "StartedPrecache", expectedPrecacheStarted, mPrecachingStarted);
                    Assert.assertEquals(
                            "FetchedSnippets", expectedFetchSnippets, mDidFetchSnippets);
                    Assert.assertEquals("RescheduledFetching", expectedRescheduleFetching,
                            mDidRescheduleFetching);
                }
            });
        }

        protected void deletePrecacheInstance() {
            mHasPrecacheInstance = false;
        }
    }

    @Before
    public void setUp() throws Exception {
        BackgroundSyncLauncher.setGCMEnabled(false);
        RecordHistogram.setDisabledForTests(true);
        mSyncLauncher = BackgroundSyncLauncher.create();
        mSnippetsLauncher = SnippetsLauncher.create();
        mTaskService = new MockTaskService();
    }

    @After
    public void tearDown() throws Exception {
        RecordHistogram.setDisabledForTests(false);
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

    @Test
    @SmallTest
    @Feature({"BackgroundSync"})
    public void testBackgroundSyncNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(BackgroundSyncLauncher.TASK_TAG, false, false, false);
    }

    @Test
    @SmallTest
    @Feature({"BackgroundSync"})
    public void testBackgroundSyncLaunchBrowserWhenInstanceDoesNotExist() {
        deleteSyncLauncherInstance();
        startOnRunTaskAndVerify(BackgroundSyncLauncher.TASK_TAG, true, false, false);
    }

    @Test
    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchWifiNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_WIFI, false, false, true);
    }

    @Test
    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchFallbackNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_FALLBACK, false, false, true);
    }

    @Test
    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchWifiLaunchBrowserWhenInstanceDoesNotExist() {
        deleteSnippetsLauncherInstance();
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_WIFI, true, false, true);
    }

    @Test
    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchFallbackLaunchBrowserWhenInstanceDoesNotExist() {
        deleteSnippetsLauncherInstance();
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_FALLBACK, true, false, true);
    }

    @Test
    @SmallTest
    @Feature({"Precache"})
    public void testPrecacheNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(PrecacheController.PERIODIC_TASK_TAG, false, false, false);
    }

    @Test
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

    @Test
    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsNoRescheduleWithoutPrefWhenInstanceExists() {
        startOnInitializeTasksAndVerify(/*shouldStart=*/false, /*shouldReschedule=*/false);
    }

    @Test
    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsNoRescheduleWithoutPrefWhenInstanceDoesNotExist() {
        deleteSnippetsLauncherInstance();
        startOnInitializeTasksAndVerify(/*shouldStart=*/false, /*shouldReschedule=*/false);
    }

    @Test
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

    @Test
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
