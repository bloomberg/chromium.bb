// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

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
import org.chromium.components.invalidation.PendingInvalidation;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.List;

/**
 * Tests for DelayedInvalidationsController.
 */
public class DelayedInvalidationsControllerTest extends ChromeShellTestBase {
    private static final String TEST_ACCOUNT = "something@gmail.com";
    private static final long WAIT_FOR_LAUNCHER_MS = scaleTimeout(10 * 1000);
    private static final long POLL_INTERVAL_MS = 100;

    private static final String OBJECT_ID = "object_id";
    private static final int OBJECT_SRC = 4;
    private static final long VERSION = 1L;
    private static final String PAYLOAD = "payload";

    private static final String OBJECT_ID_2 = "object_id_2";
    private static final int OBJECT_SRC_2 = 5;
    private static final long VERSION_2 = 2L;
    private static final String PAYLOAD_2 = "payload_2";

    private MockDelayedInvalidationsController mController;

    /**
     * Mocks {@link DelayedInvalidationsController} for testing.
     * It intercepts access to the Android Sync Adapter.
     */
    private static class MockDelayedInvalidationsController extends DelayedInvalidationsController {
        private boolean mInvalidated = false;
        private List<Bundle> mBundles = null;

        private MockDelayedInvalidationsController() {}

        @Override
        void notifyInvalidationsOnBackgroundThread(
                Context context, Account account, List<Bundle> bundles) {
            mInvalidated = true;
            mBundles = bundles;
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mController = new MockDelayedInvalidationsController();
        launchChromeShellWithBlankPage();
    }

    @SmallTest
    @Feature({"Sync"})
    public void testManualSyncRequestsShouldAlwaysTriggerSync() throws InterruptedException {
        // Sync should trigger for manual requests when Chrome is in the foreground.
        assertTrue(isActivityResumed());
        Bundle extras = new Bundle();
        extras.putBoolean(ContentResolver.SYNC_EXTRAS_MANUAL, true);
        assertTrue(mController.shouldNotifyInvalidation(extras));

        // Sync should trigger for manual requests when Chrome is in the background.
        sendChromeToBackground(getActivity());
        extras.putBoolean(ContentResolver.SYNC_EXTRAS_MANUAL, true);
        assertTrue(mController.shouldNotifyInvalidation(extras));
    }

    @SmallTest
    @Feature({"Sync", "Invalidation"})
    public void testInvalidationsTriggeredWhenChromeIsInForeground() {
        assertTrue(isActivityResumed());
        assertTrue(mController.shouldNotifyInvalidation(new Bundle()));
    }

    @SmallTest
    @Feature({"Sync", "Invalidation"})
    public void testInvalidationsReceivedWhenChromeIsInBackgroundIsDelayed()
            throws InterruptedException {
        sendChromeToBackground(getActivity());
        assertFalse(mController.shouldNotifyInvalidation(new Bundle()));
    }

    @SmallTest
    @Feature({"Sync", "Invalidation"})
    public void testOnlySpecificInvalidationsTriggeredOnResume() throws InterruptedException {
        // First make sure there are no pending invalidations.
        mController.clearPendingInvalidations(getActivity());
        assertFalse(mController.notifyPendingInvalidations(getActivity()));
        assertFalse(mController.mInvalidated);

        // Create some invalidations.
        PendingInvalidation firstInv =
                new PendingInvalidation(OBJECT_ID, OBJECT_SRC, VERSION, PAYLOAD);
        PendingInvalidation secondInv =
                new PendingInvalidation(OBJECT_ID_2, OBJECT_SRC_2, VERSION_2, PAYLOAD_2);

        // Can't invalidate while Chrome is in the background.
        sendChromeToBackground(getActivity());
        assertFalse(mController.shouldNotifyInvalidation(new Bundle()));

        // Add multiple pending invalidations.
        mController.addPendingInvalidation(getActivity(), TEST_ACCOUNT, firstInv);
        mController.addPendingInvalidation(getActivity(), TEST_ACCOUNT, secondInv);

        // Make sure there are pending invalidations.
        assertTrue(mController.notifyPendingInvalidations(getActivity()));
        assertTrue(mController.mInvalidated);

        // Ensure only specific invalidations are being notified.
        assertEquals(2, mController.mBundles.size());
        PendingInvalidation parsedInv1 = new PendingInvalidation(mController.mBundles.get(0));
        PendingInvalidation parsedInv2 = new PendingInvalidation(mController.mBundles.get(1));
        assertTrue(firstInv.equals(parsedInv1) ^ firstInv.equals(parsedInv2));
        assertTrue(secondInv.equals(parsedInv1) ^ secondInv.equals(parsedInv2));
    }

    @SmallTest
    @Feature({"Sync", "Invalidation"})
    public void testAllInvalidationsTriggeredOnResume() throws InterruptedException {
        // First make sure there are no pending invalidations.
        mController.clearPendingInvalidations(getActivity());
        assertFalse(mController.notifyPendingInvalidations(getActivity()));
        assertFalse(mController.mInvalidated);

        // Create some invalidations.
        PendingInvalidation firstInv =
                new PendingInvalidation(OBJECT_ID, OBJECT_SRC, VERSION, PAYLOAD);
        PendingInvalidation secondInv =
                new PendingInvalidation(OBJECT_ID_2, OBJECT_SRC_2, VERSION_2, PAYLOAD_2);
        PendingInvalidation allInvalidations = new PendingInvalidation(new Bundle());
        assertEquals(allInvalidations.mObjectSource, 0);

        // Can't invalidate while Chrome is in the background.
        sendChromeToBackground(getActivity());
        assertFalse(mController.shouldNotifyInvalidation(new Bundle()));

        // Add multiple pending invalidations.
        mController.addPendingInvalidation(getActivity(), TEST_ACCOUNT, firstInv);
        mController.addPendingInvalidation(getActivity(), TEST_ACCOUNT, allInvalidations);
        mController.addPendingInvalidation(getActivity(), TEST_ACCOUNT, secondInv);

        // Make sure there are pending invalidations.
        assertTrue(mController.notifyPendingInvalidations(getActivity()));
        assertTrue(mController.mInvalidated);

        // As Invalidation for all ids has been received, it will supersede all other invalidations.
        assertEquals(1, mController.mBundles.size());
        assertEquals(allInvalidations, new PendingInvalidation(mController.mBundles.get(0)));
    }

    @VisibleForTesting
    static void sendChromeToBackground(Activity activity) throws InterruptedException {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_HOME);
        activity.startActivity(intent);

        assertTrue(
                "Activity should have been resumed", CriteriaHelper.pollForCriteria(new Criteria() {
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
