// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import com.google.android.gms.gcm.TaskParams;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ntp.snippets.SnippetsLauncher;
import org.chromium.chrome.browser.precache.PrecacheController;

/**
 * Tests {@link ChromeBackgroundService}.
 */
public class ChromeBackgroundServiceTest extends InstrumentationTestCase {
    private Context mContext;
    private BackgroundSyncLauncher mSyncLauncher;
    private SnippetsLauncher mSnippetsLauncher;
    private MockTaskService mTaskService;

    static class MockTaskService extends ChromeBackgroundService {
        private boolean mDidLaunchBrowser = false;
        private boolean mDidFetchSnippets = false;
        private boolean mDidRescheduleSnippets = false;
        private boolean mHasPrecacheInstance = true;
        private boolean mPrecachingStarted = false;

        @Override
        protected void launchBrowser(Context context) {
            mDidLaunchBrowser = true;
        }

        @Override
        protected void fetchSnippets() {
            mDidFetchSnippets = true;
        }

        @Override
        protected void rescheduleSnippets() {
            mDidRescheduleSnippets = true;
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

        // Posts an assertion task to the UI thread. Since this is only called after the call
        // to onRunTask, it will be enqueued after any possible call to launchBrowser, and we
        // can reliably check whether launchBrowser was called.
        protected void checkExpectations(final boolean expectedLaunchBrowser,
                final boolean expectedPrecacheStarted, final boolean expectedFetchSnippets,
                final boolean expectedRescheduleSnippets) {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    assertEquals("StartedService", expectedLaunchBrowser, mDidLaunchBrowser);
                    assertEquals("StartedPrecache", expectedPrecacheStarted, mPrecachingStarted);
                    assertEquals("FetchedSnippets", expectedFetchSnippets, mDidFetchSnippets);
                    assertEquals("RescheduledSnippets", expectedRescheduleSnippets,
                            mDidRescheduleSnippets);
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
            boolean shouldPrecache, boolean shouldFetchSnippets, boolean shouldRescheduleSnippets) {
        mTaskService.onRunTask(new TaskParams(taskTag));
        mTaskService.checkExpectations(
                shouldStart, shouldPrecache, shouldFetchSnippets, shouldRescheduleSnippets);
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testBackgroundSyncNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(BackgroundSyncLauncher.TASK_TAG, false, false, false, false);
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testBackgroundSyncLaunchBrowserWhenInstanceDoesNotExist() {
        deleteSyncLauncherInstance();
        startOnRunTaskAndVerify(BackgroundSyncLauncher.TASK_TAG, true, false, false, false);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchWifiChargingNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_WIFI_CHARGING, false, false, true, false);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchWifiNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_WIFI, false, false, true, false);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchFallbackNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_FALLBACK, false, false, true, false);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsRescheduleNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_RESCHEDULE, false, false, false, true);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchWifiChargingLaunchBrowserWhenInstanceDoesNotExist() {
        deleteSnippetsLauncherInstance();
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_WIFI_CHARGING, true, false, true, false);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchWifiLaunchBrowserWhenInstanceDoesNotExist() {
        deleteSnippetsLauncherInstance();
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_WIFI, true, false, true, false);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchFallbackLaunchBrowserWhenInstanceDoesNotExist() {
        deleteSnippetsLauncherInstance();
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_FALLBACK, true, false, true, false);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsRescheduleLaunchBrowserWhenInstanceDoesNotExist() {
        deleteSnippetsLauncherInstance();
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_RESCHEDULE, true, false, false, true);
    }

    @SmallTest
    @Feature({"Precache"})
    public void testPrecacheNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(PrecacheController.PERIODIC_TASK_TAG, false, false, false, false);
    }

    @SmallTest
    @Feature({"Precache"})
    public void testPrecacheLaunchBrowserWhenInstanceDoesNotExist() {
        mTaskService.deletePrecacheInstance();
        startOnRunTaskAndVerify(PrecacheController.PERIODIC_TASK_TAG, true, true, false, false);
    }
}
