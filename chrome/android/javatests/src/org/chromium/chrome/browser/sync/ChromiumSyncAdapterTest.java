// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.app.Application;
import android.content.Context;
import android.content.SyncResult;
import android.os.Bundle;
import android.test.suitebuilder.annotation.MediumTest;

import com.google.protos.ipc.invalidation.Types;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.sync.notifier.SyncStatusHelper;
import org.chromium.sync.signin.AccountManagerHelper;

public class ChromiumSyncAdapterTest extends ChromeShellTestBase {

    private static final Account TEST_ACCOUNT =
            AccountManagerHelper.createAccountFromName("test@gmail.com");

    private TestChromiumSyncAdapter mSyncAdapter;

    private static class TestChromiumSyncAdapter extends ChromiumSyncAdapter {
        private boolean mCommandlineInitialized;
        private boolean mSyncRequested;
        private boolean mSyncRequestedForAllTypes;
        private int mObjectSource;
        private String mObjectId;
        private long mVersion;
        private String mPayload;

        public TestChromiumSyncAdapter(Context context, Application application) {
            super(context, application);
        }

        @Override
        protected boolean useAsyncStartup() {
            return true;
        }

        @Override
        protected void initCommandLine() {
            mCommandlineInitialized = true;
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
        launchChromeShellWithBlankPage();
        mSyncAdapter = new TestChromiumSyncAdapter(getInstrumentation().getTargetContext(),
                getActivity().getApplication());
    }

    @MediumTest
    @Feature({"Sync"})
    public void testRequestSyncNoInvalidationData() {
        SyncResult syncResult = new SyncResult();
        mSyncAdapter.onPerformSync(TEST_ACCOUNT, new Bundle(),
                SyncStatusHelper.get(getActivity()).getContractAuthority(), null, syncResult);
        assertTrue(mSyncAdapter.mSyncRequestedForAllTypes);
        assertFalse(mSyncAdapter.mSyncRequested);
        assertTrue(mSyncAdapter.mCommandlineInitialized);
    }

    private void testRequestSyncSpecificDataType(boolean withObjectSource) {
        SyncResult syncResult = new SyncResult();
        Bundle extras = new Bundle();
        if (withObjectSource) {
            extras.putInt(ChromiumSyncAdapter.INVALIDATION_OBJECT_SOURCE_KEY, 61);
        }
        extras.putString(ChromiumSyncAdapter.INVALIDATION_OBJECT_ID_KEY, "objectid_value");
        extras.putLong(ChromiumSyncAdapter.INVALIDATION_VERSION_KEY, 42);
        extras.putString(ChromiumSyncAdapter.INVALIDATION_PAYLOAD_KEY, "payload_value");
        mSyncAdapter.onPerformSync(TEST_ACCOUNT, extras,
                SyncStatusHelper.get(getActivity()).getContractAuthority(), null, syncResult);
        assertFalse(mSyncAdapter.mSyncRequestedForAllTypes);
        assertTrue(mSyncAdapter.mSyncRequested);
        if (withObjectSource) {
            assertEquals(61, mSyncAdapter.mObjectSource);
        } else {
            assertEquals(Types.ObjectSource.Type.CHROME_SYNC.getNumber(),
                    mSyncAdapter.mObjectSource);
        }
        assertEquals("objectid_value", mSyncAdapter.mObjectId);
        assertEquals(42, mSyncAdapter.mVersion);
        assertEquals("payload_value", mSyncAdapter.mPayload);
        assertTrue(mSyncAdapter.mCommandlineInitialized);
    }

    @MediumTest
    @Feature({"Sync"})
    public void testRequestSyncSpecificDataType() {
      testRequestSyncSpecificDataType(true /* withObjectSource */);
    }

    @MediumTest
    @Feature({"Sync"})
    public void testRequestSyncSpecificDataType_withoutObjectSource() {
      testRequestSyncSpecificDataType(false /* withObjectSource */);
    }

    @MediumTest
    @Feature({"Sync"})
    public void testRequestSyncWhenChromeInBackground() throws InterruptedException {
        DelayedSyncControllerTest.sendChromeToBackground(getActivity());
        SyncResult syncResult = new SyncResult();
        mSyncAdapter.onPerformSync(TEST_ACCOUNT, new Bundle(),
                SyncStatusHelper.get(getActivity()).getContractAuthority(), null, syncResult);
        assertFalse(mSyncAdapter.mSyncRequestedForAllTypes);
        assertFalse(mSyncAdapter.mSyncRequested);
        assertFalse(mSyncAdapter.mCommandlineInitialized);
    }
}
