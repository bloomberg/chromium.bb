// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.app.Application;
import android.content.ContentResolver;
import android.content.Context;
import android.content.SyncResult;
import android.os.Bundle;
import android.test.suitebuilder.annotation.MediumTest;

import com.google.protos.ipc.invalidation.Types;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.signin.AccountManagerHelper;

/**
 * Tests for ChromiumSyncAdapter.
 */
public class ChromiumSyncAdapterTest extends ChromeShellTestBase {

    private static final Account TEST_ACCOUNT =
            AccountManagerHelper.createAccountFromName("test@gmail.com");

    private TestChromiumSyncAdapter mSyncAdapter;

    private static class TestChromiumSyncAdapter extends ChromiumSyncAdapter {
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

    public void performSyncWithBundle(Bundle bundle) {
        mSyncAdapter.onPerformSync(TEST_ACCOUNT, bundle,
                AndroidSyncSettings.get(getActivity()).getContractAuthority(),
                null, new SyncResult());
    }

    @MediumTest
    @Feature({"Sync"})
    public void testRequestSyncNoInvalidationData() {
        performSyncWithBundle(new Bundle());
        assertTrue(mSyncAdapter.mSyncRequestedForAllTypes);
        assertFalse(mSyncAdapter.mSyncRequested);
        assertTrue(CommandLine.isInitialized());
    }

    private void testRequestSyncSpecificDataType(boolean withObjectSource) {
        Bundle extras = new Bundle();
        if (withObjectSource) {
            extras.putInt(ChromiumSyncAdapter.INVALIDATION_OBJECT_SOURCE_KEY, 61);
        }
        extras.putString(ChromiumSyncAdapter.INVALIDATION_OBJECT_ID_KEY, "objectid_value");
        extras.putLong(ChromiumSyncAdapter.INVALIDATION_VERSION_KEY, 42);
        extras.putString(ChromiumSyncAdapter.INVALIDATION_PAYLOAD_KEY, "payload_value");

        performSyncWithBundle(extras);

        assertFalse(mSyncAdapter.mSyncRequestedForAllTypes);
        assertTrue(mSyncAdapter.mSyncRequested);
        if (withObjectSource) {
            assertEquals(61, mSyncAdapter.mObjectSource);
        } else {
            assertEquals(Types.ObjectSource.CHROME_SYNC, mSyncAdapter.mObjectSource);
        }
        assertEquals("objectid_value", mSyncAdapter.mObjectId);
        assertEquals(42, mSyncAdapter.mVersion);
        assertEquals("payload_value", mSyncAdapter.mPayload);
        assertTrue(CommandLine.isInitialized());
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
        performSyncWithBundle(new Bundle());
        assertFalse(mSyncAdapter.mSyncRequestedForAllTypes);
        assertFalse(mSyncAdapter.mSyncRequested);
        assertTrue(CommandLine.isInitialized());
    }

    @MediumTest
    @Feature({"Sync"})
    public void testRequestInitializeSync() throws InterruptedException {
        Bundle extras = new Bundle();
        extras.putBoolean(ContentResolver.SYNC_EXTRAS_INITIALIZE, true);
        performSyncWithBundle(extras);
        assertFalse(mSyncAdapter.mSyncRequestedForAllTypes);
        assertFalse(mSyncAdapter.mSyncRequested);
    }
}
