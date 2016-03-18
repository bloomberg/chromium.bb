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
import org.chromium.chrome.browser.ntp.snippets.SnippetsController;
import org.chromium.chrome.browser.ntp.snippets.SnippetsLauncher;

/**
 * Tests {@link ChromeBackgroundService}.
 */
public class ChromeBackgroundServiceTest extends InstrumentationTestCase {
    private Context mContext;
    private BackgroundSyncLauncher mSyncLauncher;
    private SnippetsLauncher mSnippetsLauncher;
    private MockSnippetsController mSnippetsController;
    private MockTaskService mTaskService;

    static class MockTaskService extends ChromeBackgroundService {
        private boolean mDidLaunchBrowser = false;

        @Override
        protected void launchBrowser(Context context) {
            mDidLaunchBrowser = true;
        }

        // Posts an assertion task to the UI thread. Since this is only called after the call
        // to onRunTask, it will be enqueued after any possible call to launchBrowser, and we
        // can reliably check whether launchBrowser was called.
        protected void checkExpectations(final boolean expectedLaunchBrowser) {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    assertEquals("StartedService", expectedLaunchBrowser, mDidLaunchBrowser);
                }
            });
        }
    }

    static class MockSnippetsController extends SnippetsController {
        private boolean mDidFetchSnippets = false;

        MockSnippetsController() {
            super(null);
        }

        @Override
        public void fetchSnippets() {
            mDidFetchSnippets = true;
        }

        protected void checkExpectations(final boolean expectedFetchSnippets) {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    assertEquals("FetchedSnippets", expectedFetchSnippets, mDidFetchSnippets);
                }
            });
        }
    }

    @Override
    protected void setUp() throws Exception {
        mContext = new AdvancedMockContext(getInstrumentation().getTargetContext());
        BackgroundSyncLauncher.setGCMEnabled(false);
        RecordHistogram.disableForTests();
        mSyncLauncher = BackgroundSyncLauncher.create(mContext);
        mSnippetsLauncher = SnippetsLauncher.create(mContext);
        mSnippetsController = new MockSnippetsController();
        SnippetsController.setInstanceForTesting(mSnippetsController);
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
            boolean shouldFetchSnippets) {
        mTaskService.onRunTask(new TaskParams(taskTag));
        mTaskService.checkExpectations(shouldStart);
        mSnippetsController.checkExpectations(shouldFetchSnippets);
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testBackgroundSyncNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(BackgroundSyncLauncher.TASK_TAG, false, false);
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testBackgroundSyncLaunchBrowserWhenInstanceDoesNotExist() {
        deleteSyncLauncherInstance();
        startOnRunTaskAndVerify(BackgroundSyncLauncher.TASK_TAG, true, false);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_WIFI_CHARGING, false, true);
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_WIFI, false, true);
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_FALLBACK, false, true);
    }

    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsLaunchBrowserWhenInstanceDoesNotExist() {
        deleteSnippetsLauncherInstance();
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_WIFI_CHARGING, true, true);
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_WIFI, true, true);
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_FALLBACK, true, true);
    }
}
