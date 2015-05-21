// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

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


    private static class TestChromeSyncAdapter extends ChromiumSyncAdapter {
        private boolean mSyncRequested;
        private boolean mSyncRequestedForAllTypes;
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
        public void requestSync(int objectSource, String objectId, long version, String payload) {
            mObjectSource = objectSource;
            mObjectId = objectId;
            mVersion = version;
            mPayload = payload;
            mSyncRequested = true;
        }

        @Override
        public void requestSyncForAllTypes() {
            mSyncRequestedForAllTypes = true;
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mSyncAdapter = new TestChromeSyncAdapter(getInstrumentation().getTargetContext(),
                getActivity().getApplication());
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @MediumTest
    @Feature({"Sync"})
    public void testRequestSyncNoInvalidationData() {
        SyncResult syncResult = new SyncResult();
        mSyncAdapter.onPerformSync(TEST_ACCOUNT, new Bundle(),
                AndroidSyncSettings.getContractAuthority(getActivity()), null, syncResult);
        assertTrue(mSyncAdapter.mSyncRequestedForAllTypes);
        assertFalse(mSyncAdapter.mSyncRequested);
        assertTrue(CommandLine.isInitialized());
    }

    @MediumTest
    @Feature({"Sync"})
    public void testRequestSyncSpecificDataType() {
        SyncResult syncResult = new SyncResult();
        Bundle extras = new Bundle();
        extras.putInt(ChromiumSyncAdapter.INVALIDATION_OBJECT_SOURCE_KEY, 65);
        extras.putString(ChromiumSyncAdapter.INVALIDATION_OBJECT_ID_KEY, "objectid_value");
        extras.putLong(ChromiumSyncAdapter.INVALIDATION_VERSION_KEY, 42);
        extras.putString(ChromiumSyncAdapter.INVALIDATION_PAYLOAD_KEY, "payload_value");
        mSyncAdapter.onPerformSync(TEST_ACCOUNT, extras,
                AndroidSyncSettings.getContractAuthority(getActivity()), null, syncResult);
        assertFalse(mSyncAdapter.mSyncRequestedForAllTypes);
        assertTrue(mSyncAdapter.mSyncRequested);
        assertEquals(65, mSyncAdapter.mObjectSource);
        assertEquals("objectid_value", mSyncAdapter.mObjectId);
        assertEquals(42, mSyncAdapter.mVersion);
        assertEquals("payload_value", mSyncAdapter.mPayload);
        assertTrue(CommandLine.isInitialized());
    }

    @MediumTest
    @Feature({"Sync"})
    public void testRequestSyncWhenChromeInBackground() throws InterruptedException {
        sendChromeToBackground(getActivity());
        SyncResult syncResult = new SyncResult();
        mSyncAdapter.onPerformSync(TEST_ACCOUNT, new Bundle(),
                AndroidSyncSettings.getContractAuthority(getActivity()), null, syncResult);
        assertFalse(mSyncAdapter.mSyncRequestedForAllTypes);
        assertFalse(mSyncAdapter.mSyncRequested);
    }
}
