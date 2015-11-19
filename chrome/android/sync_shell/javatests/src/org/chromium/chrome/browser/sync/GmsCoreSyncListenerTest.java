// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Test suite for the GmsCoreSyncListener.
 */
public class GmsCoreSyncListenerTest extends SyncTestBase {
    private static final String PASSPHRASE = "passphrase";

    static class CountingGmsCoreSyncListener extends GmsCoreSyncListener {
        private int mCallCount = 0;

        @Override
        public void updateEncryptionKey(byte[] key) {
            mCallCount++;
        }

        public int callCount() {
            return mCallCount;
        }
    }

    private CountingGmsCoreSyncListener mListener;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mListener = new CountingGmsCoreSyncListener();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ProfileSyncService.get().addSyncStateChangedListener(mListener);
            }
        });
    }

    @Override
    protected void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ProfileSyncService.get().removeSyncStateChangedListener(mListener);
            }
        });
        super.tearDown();
    }

    @MediumTest
    @Feature({"Sync"})
    public void testGetsKey() throws Throwable {
        Account account = setUpTestAccountAndSignInToSync();
        assertEquals(0, mListener.callCount());
        encryptWithPassphrase(PASSPHRASE);
        waitForCallCount(1);
        signOut();
        signIn(account);
        assertEquals(1, mListener.callCount());
        decryptWithPassphrase(PASSPHRASE);
        waitForCallCount(2);
    }

    @MediumTest
    @Feature({"Sync"})
    public void testClearData() throws Throwable {
        setUpTestAccountAndSignInToSync();
        assertEquals(0, mListener.callCount());
        encryptWithPassphrase(PASSPHRASE);
        waitForCallCount(1);
        clearServerData();
        startSync();
        encryptWithPassphrase(PASSPHRASE);
        waitForCallCount(2);
    }

    private void encryptWithPassphrase(final String passphrase) throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ProfileSyncService.get().setEncryptionPassphrase(passphrase);
            }
        });
        waitForCryptographer();
        // Make sure the new encryption settings make it to the server.
        SyncTestUtil.triggerSyncAndWaitForCompletion();
    }

    private void decryptWithPassphrase(final String passphrase) throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ProfileSyncService.get().setDecryptionPassphrase(passphrase);
            }
        });
    }

    private void waitForCryptographer() throws InterruptedException {
        boolean isReady = CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ProfileSyncService syncService = ProfileSyncService.get();
                return syncService.isUsingSecondaryPassphrase()
                        && syncService.isCryptographerReady();
            }
        });
        assertTrue("Timed out waiting for cryptographer to be ready.", isReady);
    }

    private void waitForCallCount(final int count) throws InterruptedException {
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mListener.callCount() == count;
            }
        });
        assertEquals(count, mListener.callCount());
    }
}
