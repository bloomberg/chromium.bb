// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import android.accounts.Account;
import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.content.SyncResult;
import android.os.Bundle;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.components.invalidation.PendingInvalidation;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.signin.AccountManagerHelper;

/**
 * Tests for ChromeBrowserSyncAdapter.
 *
 * TODO(nyquist) Remove this class when Chrome sync starts up the same way as the testshell.
 */
public class ChromeBrowserSyncAdapterTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final Account TEST_ACCOUNT =
            AccountManagerHelper.createAccountFromName("test@gmail.com");
    private static final long WAIT_FOR_LAUNCHER_MS = 10 * 1000;
    private static final long POLL_INTERVAL_MS = 100;

    private TestChromeSyncAdapter mSyncAdapter;

    public ChromeBrowserSyncAdapterTest() {
        super(ChromeActivity.class);
    }

    private static void sendChromeToBackground(Context context) throws InterruptedException {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_HOME);
        context.startActivity(intent);

        assertTrue("Activity should have been sent to background",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return !ApplicationStatus.hasVisibleActivities();
                    }
                }, WAIT_FOR_LAUNCHER_MS, POLL_INTERVAL_MS));
    }

    private void performSyncWithBundle(Bundle bundle) {
        mSyncAdapter.onPerformSync(TEST_ACCOUNT, bundle,
                AndroidSyncSettings.getContractAuthority(getActivity()), null, new SyncResult());
    }

    private static class TestChromeSyncAdapter extends ChromiumSyncAdapter {
        private boolean mInvalidationRequested;
        private boolean mInvalidationRequestedForAllTypes;
        private int mObjectSource;
        private String mObjectId;
        private long mVersion;
        private String mPayload;

        public TestChromeSyncAdapter(Context context, Application application) {
            super(context, application);
        }

        @Override
        protected boolean useAsyncStartup() {
            return false;
        }

        @Override
        public void notifyInvalidation(
                int objectSource, String objectId, long version, String payload) {
            mObjectSource = objectSource;
            mObjectId = objectId;
            mVersion = version;
            mPayload = payload;
            if (objectSource == 0) {
                mInvalidationRequestedForAllTypes = true;
            } else {
                mInvalidationRequested = true;
            }
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mSyncAdapter = new TestChromeSyncAdapter(
                getInstrumentation().getTargetContext(), getActivity().getApplication());
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @MediumTest
    @Feature({"Sync", "Invalidation"})
    public void testRequestInvalidationForAllTypes() {
        performSyncWithBundle(new Bundle());
        assertTrue(mSyncAdapter.mInvalidationRequestedForAllTypes);
        assertFalse(mSyncAdapter.mInvalidationRequested);
        assertTrue(CommandLine.isInitialized());
    }

    @MediumTest
    @Feature({"Sync", "Invalidation"})
    public void testRequestSpecificInvalidation() {
        String objectId = "objectid_value";
        int objectSource = 65;
        long version = 42L;
        String payload = "payload_value";

        performSyncWithBundle(
                PendingInvalidation.createBundle(objectId, objectSource, version, payload));

        assertFalse(mSyncAdapter.mInvalidationRequestedForAllTypes);
        assertTrue(mSyncAdapter.mInvalidationRequested);
        assertEquals(objectSource, mSyncAdapter.mObjectSource);
        assertEquals(objectId, mSyncAdapter.mObjectId);
        assertEquals(version, mSyncAdapter.mVersion);
        assertEquals(payload, mSyncAdapter.mPayload);
        assertTrue(CommandLine.isInitialized());
    }

    @MediumTest
    @Feature({"Sync", "Invalidation"})
    public void testInvalidationsHeldWhenChromeInBackground() throws InterruptedException {
        sendChromeToBackground(getActivity());
        performSyncWithBundle(new Bundle());
        assertFalse(mSyncAdapter.mInvalidationRequestedForAllTypes);
        assertFalse(mSyncAdapter.mInvalidationRequested);
    }
}
