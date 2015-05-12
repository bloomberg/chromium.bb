// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.accounts.Account;
import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.sync.signin.AccountManagerHelper;

public class DelayedSyncControllerTest extends ChromeShellTestBase {
    private static final Account TEST_ACCOUNT =
            AccountManagerHelper.createAccountFromName("something@gmail.com");
    private static final long WAIT_FOR_LAUNCHER_MS = scaleTimeout(10 * 1000);
    private static final long POLL_INTERVAL_MS = 100;
    private TestDelayedSyncController mController;

    private static class TestDelayedSyncController extends DelayedSyncController {
        private boolean mSyncRequested;

        private TestDelayedSyncController() {}

        @Override
        void requestSyncOnBackgroundThread(Context context, Account account) {
            mSyncRequested = true;
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mController = new TestDelayedSyncController();
        launchChromeShellWithBlankPage();
    }

    @SmallTest
    @Feature({"Sync"})
    public void testManualSyncRequestsShouldAlwaysTriggerSync() throws InterruptedException {
        // Sync should trigger for manual requests when Chrome is in the foreground.
        assertTrue(isActivityResumed());
        Bundle extras = new Bundle();
        extras.putBoolean(ContentResolver.SYNC_EXTRAS_MANUAL, true);
        assertTrue(mController.shouldPerformSync(getActivity(), extras, TEST_ACCOUNT));

        // Sync should trigger for manual requests when Chrome is in the background.
        sendChromeToBackground(getActivity());
        extras.putBoolean(ContentResolver.SYNC_EXTRAS_MANUAL, true);
        assertTrue(mController.shouldPerformSync(getActivity(), extras, TEST_ACCOUNT));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testSyncRequestsShouldTriggerSyncWhenChromeIsInForeground() {
        assertTrue(isActivityResumed());
        Bundle extras = new Bundle();
        assertTrue(mController.shouldPerformSync(getActivity(), extras, TEST_ACCOUNT));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testSyncRequestsWhenChromeIsInBackgroundShouldBeDelayed()
            throws InterruptedException {
        sendChromeToBackground(getActivity());
        Bundle extras = new Bundle();
        assertFalse(mController.shouldPerformSync(getActivity(), extras, TEST_ACCOUNT));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testDelayedSyncRequestsShouldBeTriggeredOnResume() throws InterruptedException {
        // First make sure there are no delayed syncs.
        mController.clearDelayedSyncs(getActivity());
        assertFalse(mController.resumeDelayedSyncs(getActivity()));
        assertFalse(mController.mSyncRequested);

        // Trying to perform sync when Chrome is in the background should create a delayed sync.
        sendChromeToBackground(getActivity());
        Bundle extras = new Bundle();
        assertFalse(mController.shouldPerformSync(getActivity(), extras, TEST_ACCOUNT));

        // Make sure the delayed sync can be resumed.
        assertTrue(mController.resumeDelayedSyncs(getActivity()));
        assertTrue(mController.mSyncRequested);
    }

    @VisibleForTesting
    static void sendChromeToBackground(Activity activity) throws InterruptedException {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_HOME);
        activity.startActivity(intent);

        assertTrue("Activity should have been resumed",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return !isActivityResumed();
                    }
                }, WAIT_FOR_LAUNCHER_MS, POLL_INTERVAL_MS));
    }

    private static boolean isActivityResumed() {
        return ApplicationStatus.hasVisibleActivities();
    }
}
