// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;

/**
 * Test suite for the GmsCoreSyncListener.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisabledTest // crbug.com/789947
public class GmsCoreSyncListenerTest {
    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule();

    private static final String PASSPHRASE = "passphrase";

    static class CountingGmsCoreSyncListener extends GmsCoreSyncListener {
        private int mCallCount;

        @Override
        public void updateEncryptionKey(byte[] key) {
            mCallCount++;
        }

        public int callCount() {
            return mCallCount;
        }
    }

    private CountingGmsCoreSyncListener mListener;

    @Before
    public void setUp() throws Exception {
        mListener = new CountingGmsCoreSyncListener();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ProfileSyncService.get().addSyncStateChangedListener(mListener);
            }
        });
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ProfileSyncService.get().removeSyncStateChangedListener(mListener);
            }
        });
    }

    @Test
    @MediumTest
    @Feature({"Sync"})
    public void testGetsKey() throws Throwable {
        Account account = mSyncTestRule.setUpTestAccountAndSignIn();
        Assert.assertEquals(0, mListener.callCount());
        encryptWithPassphrase(PASSPHRASE);
        waitForCallCount(1);
        mSyncTestRule.signOut();
        mSyncTestRule.signIn(account);
        Assert.assertEquals(1, mListener.callCount());
        decryptWithPassphrase(PASSPHRASE);
        waitForCallCount(2);
    }

    @Test
    @MediumTest
    @Feature({"Sync"})
    public void testClearData() throws Throwable {
        Account account = mSyncTestRule.setUpTestAccountAndSignIn();
        Assert.assertEquals(0, mListener.callCount());
        encryptWithPassphrase(PASSPHRASE);
        waitForCallCount(1);
        mSyncTestRule.clearServerData();
        mSyncTestRule.signIn(account);
        encryptWithPassphrase(PASSPHRASE);
        waitForCallCount(2);
    }

    private void encryptWithPassphrase(final String passphrase) {
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

    private void decryptWithPassphrase(final String passphrase) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ProfileSyncService.get().setDecryptionPassphrase(passphrase);
            }
        });
    }

    private void waitForCryptographer() {
        CriteriaHelper.pollUiThread(new Criteria(
                "Timed out waiting for cryptographer to be ready.") {
            @Override
            public boolean isSatisfied() {
                ProfileSyncService syncService = ProfileSyncService.get();
                return syncService.isUsingSecondaryPassphrase()
                        && syncService.isCryptographerReady();
            }
        });
    }

    private void waitForCallCount(int count) {
        CriteriaHelper.pollUiThread(Criteria.equals(count, new Callable<Integer>() {
            @Override
            public Integer call() {
                return mListener.callCount();
            }
        }));
    }
}
