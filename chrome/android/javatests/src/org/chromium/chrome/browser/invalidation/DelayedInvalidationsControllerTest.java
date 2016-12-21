// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import android.accounts.Account;
import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.os.Bundle;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.Feature;
import org.chromium.components.invalidation.PendingInvalidation;

import java.util.List;

/**
 * Tests for DelayedInvalidationsController.
 */
public class DelayedInvalidationsControllerTest extends InstrumentationTestCase {
    private static final String TEST_ACCOUNT = "something@gmail.com";

    private static final String OBJECT_ID = "object_id";
    private static final int OBJECT_SRC = 4;
    private static final long VERSION = 1L;
    private static final String PAYLOAD = "payload";

    private static final String OBJECT_ID_2 = "object_id_2";
    private static final int OBJECT_SRC_2 = 5;
    private static final long VERSION_2 = 2L;
    private static final String PAYLOAD_2 = "payload_2";

    private MockDelayedInvalidationsController mController;
    private Context mContext;
    private Activity mPlaceholderActivity;

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
        mContext = getInstrumentation().getTargetContext();

        mPlaceholderActivity = new Activity();
        setApplicationState(ActivityState.CREATED);
        setApplicationState(ActivityState.RESUMED);
    }

    private void setApplicationState(int newState) {
        ApplicationStatus.onStateChangeForTesting(mPlaceholderActivity, newState);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testManualSyncRequestsShouldAlwaysTriggerSync() throws InterruptedException {
        // Sync should trigger for manual requests when Chrome is in the foreground.
        Bundle extras = new Bundle();
        extras.putBoolean(ContentResolver.SYNC_EXTRAS_MANUAL, true);
        assertTrue(mController.shouldNotifyInvalidation(extras));

        // Sync should trigger for manual requests when Chrome is in the background.
        setApplicationState(ActivityState.STOPPED);
        extras.putBoolean(ContentResolver.SYNC_EXTRAS_MANUAL, true);
        assertTrue(mController.shouldNotifyInvalidation(extras));
    }

    @SmallTest
    @Feature({"Sync", "Invalidation"})
    public void testInvalidationsTriggeredWhenChromeIsInForeground() {
        assertTrue(mController.shouldNotifyInvalidation(new Bundle()));
    }

    @SmallTest
    @Feature({"Sync", "Invalidation"})
    public void testInvalidationsReceivedWhenChromeIsInBackgroundIsDelayed()
            throws InterruptedException {
        setApplicationState(ActivityState.STOPPED);
        assertFalse(mController.shouldNotifyInvalidation(new Bundle()));
    }

    @SmallTest
    @Feature({"Sync", "Invalidation"})
    public void testOnlySpecificInvalidationsTriggeredOnResume() throws InterruptedException {
        // First make sure there are no pending invalidations.
        mController.clearPendingInvalidations(mContext);
        assertFalse(mController.notifyPendingInvalidations(mContext));
        assertFalse(mController.mInvalidated);

        // Create some invalidations.
        PendingInvalidation firstInv =
                new PendingInvalidation(OBJECT_ID, OBJECT_SRC, VERSION, PAYLOAD);
        PendingInvalidation secondInv =
                new PendingInvalidation(OBJECT_ID_2, OBJECT_SRC_2, VERSION_2, PAYLOAD_2);

        // Can't invalidate while Chrome is in the background.
        setApplicationState(ActivityState.STOPPED);
        assertFalse(mController.shouldNotifyInvalidation(new Bundle()));

        // Add multiple pending invalidations.
        mController.addPendingInvalidation(mContext, TEST_ACCOUNT, firstInv);
        mController.addPendingInvalidation(mContext, TEST_ACCOUNT, secondInv);

        // Make sure there are pending invalidations.
        assertTrue(mController.notifyPendingInvalidations(mContext));
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
        mController.clearPendingInvalidations(mContext);
        assertFalse(mController.notifyPendingInvalidations(mContext));
        assertFalse(mController.mInvalidated);

        // Create some invalidations.
        PendingInvalidation firstInv =
                new PendingInvalidation(OBJECT_ID, OBJECT_SRC, VERSION, PAYLOAD);
        PendingInvalidation secondInv =
                new PendingInvalidation(OBJECT_ID_2, OBJECT_SRC_2, VERSION_2, PAYLOAD_2);
        PendingInvalidation allInvalidations = new PendingInvalidation(new Bundle());
        assertEquals(allInvalidations.mObjectSource, 0);

        // Can't invalidate while Chrome is in the background.
        setApplicationState(ActivityState.STOPPED);
        assertFalse(mController.shouldNotifyInvalidation(new Bundle()));

        // Add multiple pending invalidations.
        mController.addPendingInvalidation(mContext, TEST_ACCOUNT, firstInv);
        mController.addPendingInvalidation(mContext, TEST_ACCOUNT, allInvalidations);
        mController.addPendingInvalidation(mContext, TEST_ACCOUNT, secondInv);

        // Make sure there are pending invalidations.
        assertTrue(mController.notifyPendingInvalidations(mContext));
        assertTrue(mController.mInvalidated);

        // As Invalidation for all ids has been received, it will supersede all other invalidations.
        assertEquals(1, mController.mBundles.size());
        assertEquals(allInvalidations, new PendingInvalidation(mController.mBundles.get(0)));
    }
}
