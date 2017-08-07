// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync;

import android.accounts.Account;
import android.content.Context;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.components.sync.AndroidSyncSettings.AndroidSyncSettingsObserver;
import org.chromium.components.sync.test.util.MockSyncContentResolverDelegate;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Tests for AndroidSyncSettings.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class AndroidSyncSettingsTest {
    private static class CountingMockSyncContentResolverDelegate
            extends MockSyncContentResolverDelegate {
        private final AtomicInteger mGetMasterSyncAutomaticallyCalls = new AtomicInteger();
        private final AtomicInteger mGetSyncAutomaticallyCalls = new AtomicInteger();
        private final AtomicInteger mGetIsSyncableCalls = new AtomicInteger();
        private final AtomicInteger mSetIsSyncableCalls = new AtomicInteger();
        private final AtomicInteger mSetSyncAutomaticallyCalls = new AtomicInteger();
        private final AtomicInteger mRemovePeriodicSyncCalls = new AtomicInteger();

        @Override
        public boolean getMasterSyncAutomatically() {
            mGetMasterSyncAutomaticallyCalls.getAndIncrement();
            return super.getMasterSyncAutomatically();
        }

        @Override
        public boolean getSyncAutomatically(Account account, String authority) {
            mGetSyncAutomaticallyCalls.getAndIncrement();
            return super.getSyncAutomatically(account, authority);
        }

        @Override
        public int getIsSyncable(Account account, String authority) {
            mGetIsSyncableCalls.getAndIncrement();
            return super.getIsSyncable(account, authority);
        }

        @Override
        public void setIsSyncable(Account account, String authority, int syncable) {
            mSetIsSyncableCalls.getAndIncrement();
            super.setIsSyncable(account, authority, syncable);
        }

        @Override
        public void setSyncAutomatically(Account account, String authority, boolean sync) {
            mSetSyncAutomaticallyCalls.getAndIncrement();
            super.setSyncAutomatically(account, authority, sync);
        }

        @Override
        public void removePeriodicSync(Account account, String authority, Bundle extras) {
            mRemovePeriodicSyncCalls.getAndIncrement();
            super.removePeriodicSync(account, authority, extras);
        }
    }

    private static class MockSyncSettingsObserver implements AndroidSyncSettingsObserver {
        private boolean mReceivedNotification;

        public void clearNotification() {
            mReceivedNotification = false;
        }

        public boolean receivedNotification() {
            return mReceivedNotification;
        }

        @Override
        public void androidSyncSettingsChanged() {
            mReceivedNotification = true;
        }
    }

    private Context mContext;
    private CountingMockSyncContentResolverDelegate mSyncContentResolverDelegate;
    private String mAuthority;
    private Account mAccount;
    private Account mAlternateAccount;
    private MockSyncSettingsObserver mSyncSettingsObserver;
    private FakeAccountManagerDelegate mAccountManager;
    private CallbackHelper mCallbackHelper;
    private int mNumberOfCallsToWait;

    @Before
    public void setUp() throws Exception {
        mNumberOfCallsToWait = 0;
        mCallbackHelper = new CallbackHelper();
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        setupTestAccounts(mContext);
        // Set signed in account to mAccount before initializing AndroidSyncSettings to let
        // AndroidSyncSettings establish correct assumptions.
        ChromeSigninController.get().setSignedInAccountName(mAccount.name);

        mSyncContentResolverDelegate = new CountingMockSyncContentResolverDelegate();
        AndroidSyncSettings.overrideForTests(mContext, mSyncContentResolverDelegate);
        mAuthority = AndroidSyncSettings.getContractAuthority(mContext);
        Assert.assertEquals(1, mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority));

        mSyncSettingsObserver = new MockSyncSettingsObserver();
        AndroidSyncSettings.registerObserver(mContext, mSyncSettingsObserver);

    }

    private void setupTestAccounts(Context context) {
        mAccountManager = new FakeAccountManagerDelegate(context);
        AccountManagerFacade.overrideAccountManagerFacadeForTests(context, mAccountManager);
        mAccount = setupTestAccount("account@example.com");
        mAlternateAccount = setupTestAccount("alternate@example.com");
    }

    private Account setupTestAccount(String accountName) {
        Account account = AccountManagerFacade.createAccountFromName(accountName);
        AccountHolder.Builder accountHolder =
                AccountHolder.builder(account).password("password").alwaysAccept(true);
        mAccountManager.addAccountHolderExplicitly(accountHolder.build());
        return account;
    }

    @After
    public void tearDown() throws Exception {
        if (mNumberOfCallsToWait > 0) mCallbackHelper.waitForCallback(0, mNumberOfCallsToWait);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                AccountManagerFacade.resetAccountManagerFacadeForTests();
            }
        });
    }

    private void enableChromeSyncOnUiThread() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                AndroidSyncSettings.enableChromeSync(mContext);
            }
        });
    }

    private void disableChromeSyncOnUiThread() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                AndroidSyncSettings.disableChromeSync(mContext);
            }
        });
    }

    private void updateAccountSync(Account account) throws InterruptedException, TimeoutException {
        updateAccount(account);
        mCallbackHelper.waitForCallback(0, mNumberOfCallsToWait);
    }

    private void updateAccount(Account account) {
        updateAccountWithCallback(account, new Callback<Boolean>() {
            @Override
            public void onResult(Boolean result) {
                mCallbackHelper.notifyCalled();
            }
        });
    }

    private void updateAccountWithCallback(Account account, Callback<Boolean> callback) {
        AndroidSyncSettings.updateAccount(mContext, account, callback);
        mNumberOfCallsToWait++;
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testAccountInitialization() throws InterruptedException, TimeoutException {
        // mAccount was set to be syncable and not have periodic syncs.
        Assert.assertEquals(1, mSyncContentResolverDelegate.mSetIsSyncableCalls.get());
        Assert.assertEquals(1, mSyncContentResolverDelegate.mRemovePeriodicSyncCalls.get());

        updateAccountSync(null);

        // mAccount was set to be not syncable.
        Assert.assertEquals(2, mSyncContentResolverDelegate.mSetIsSyncableCalls.get());
        Assert.assertEquals(1, mSyncContentResolverDelegate.mRemovePeriodicSyncCalls.get());
        updateAccount(mAlternateAccount);
        // mAlternateAccount was set to be syncable and not have periodic syncs.
        Assert.assertEquals(3, mSyncContentResolverDelegate.mSetIsSyncableCalls.get());
        Assert.assertEquals(2, mSyncContentResolverDelegate.mRemovePeriodicSyncCalls.get());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testToggleMasterSyncFromSettings() throws InterruptedException {
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        Assert.assertTrue(
                "master sync should be set", AndroidSyncSettings.isMasterSyncEnabled(mContext));

        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        Assert.assertFalse(
                "master sync should be unset", AndroidSyncSettings.isMasterSyncEnabled(mContext));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testToggleChromeSyncFromSettings() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        // First sync
        mSyncContentResolverDelegate.setIsSyncable(mAccount, mAuthority, 1);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        Assert.assertTrue("sync should be set", AndroidSyncSettings.isSyncEnabled(mContext));
        Assert.assertTrue("sync should be set for chrome app",
                AndroidSyncSettings.isChromeSyncEnabled(mContext));

        // Disable sync automatically for the app
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, false);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        Assert.assertFalse("sync should be unset", AndroidSyncSettings.isSyncEnabled(mContext));
        Assert.assertFalse("sync should be unset for chrome app",
                AndroidSyncSettings.isChromeSyncEnabled(mContext));

        // Re-enable sync
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        Assert.assertTrue("sync should be re-enabled", AndroidSyncSettings.isSyncEnabled(mContext));
        Assert.assertTrue("sync should be set for chrome app",
                AndroidSyncSettings.isChromeSyncEnabled(mContext));

        // Disabled from master sync
        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        Assert.assertFalse("sync should be disabled due to master sync",
                AndroidSyncSettings.isSyncEnabled(mContext));
        Assert.assertFalse("master sync should be disabled",
                AndroidSyncSettings.isMasterSyncEnabled(mContext));
        Assert.assertTrue("sync should be set for chrome app",
                AndroidSyncSettings.isChromeSyncEnabled(mContext));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testToggleAccountSyncFromApplication() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        enableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        Assert.assertTrue("account should be synced", AndroidSyncSettings.isSyncEnabled(mContext));

        disableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        Assert.assertFalse(
                "account should not be synced", AndroidSyncSettings.isSyncEnabled(mContext));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testToggleSyncabilityForMultipleAccounts() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        enableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        Assert.assertTrue("account should be synced", AndroidSyncSettings.isSyncEnabled(mContext));

        updateAccount(mAlternateAccount);
        enableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        Assert.assertTrue(
                "alternate account should be synced", AndroidSyncSettings.isSyncEnabled(mContext));

        disableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        Assert.assertFalse("alternate account should not be synced",
                AndroidSyncSettings.isSyncEnabled(mContext));
        updateAccount(mAccount);
        Assert.assertTrue(
                "account should still be synced", AndroidSyncSettings.isSyncEnabled(mContext));

        // Ensure we don't erroneously re-use cached data.
        updateAccount(null);
        Assert.assertFalse(
                "null account should not be synced", AndroidSyncSettings.isSyncEnabled(mContext));
    }

    // TODO(crbug/737862): This test is flaky. When it fails, append report to the bug.
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSyncSettingsCaching() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        enableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        Assert.assertTrue("account should be synced", AndroidSyncSettings.isSyncEnabled(mContext));

        int masterSyncAutomaticallyCalls =
                mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls.get();
        int isSyncableCalls = mSyncContentResolverDelegate.mGetIsSyncableCalls.get();
        int getSyncAutomaticallyAcalls =
                mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls.get();

        // Do a bunch of reads.
        AndroidSyncSettings.isMasterSyncEnabled(mContext);
        AndroidSyncSettings.isSyncEnabled(mContext);
        AndroidSyncSettings.isChromeSyncEnabled(mContext);

        // Ensure values were read from cache.
        Assert.assertEquals(masterSyncAutomaticallyCalls,
                mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls.get());
        Assert.assertEquals(
                isSyncableCalls, mSyncContentResolverDelegate.mGetIsSyncableCalls.get());
        Assert.assertEquals(getSyncAutomaticallyAcalls,
                mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls.get());

        // Do a bunch of reads for alternate account.
        updateAccount(mAlternateAccount);
        AndroidSyncSettings.isMasterSyncEnabled(mContext);
        AndroidSyncSettings.isSyncEnabled(mContext);
        AndroidSyncSettings.isChromeSyncEnabled(mContext);

        // Ensure settings were only fetched once.
        Assert.assertEquals(masterSyncAutomaticallyCalls + 1,
                mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls.get());
        Assert.assertEquals(
                isSyncableCalls + 1, mSyncContentResolverDelegate.mGetIsSyncableCalls.get());
        Assert.assertEquals(getSyncAutomaticallyAcalls + 1,
                mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls.get());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testGetContractAuthority() throws Exception {
        Assert.assertEquals("The contract authority should be the package name.",
                InstrumentationRegistry.getInstrumentation().getTargetContext().getPackageName(),
                AndroidSyncSettings.getContractAuthority(mContext));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testAndroidSyncSettingsPostsNotifications() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        mSyncSettingsObserver.clearNotification();
        AndroidSyncSettings.enableChromeSync(mContext);
        Assert.assertTrue("enableChromeSync should trigger observers",
                mSyncSettingsObserver.receivedNotification());

        mSyncSettingsObserver.clearNotification();
        updateAccount(mAlternateAccount);
        Assert.assertTrue("switching to account with different settings should notify",
                mSyncSettingsObserver.receivedNotification());

        mSyncSettingsObserver.clearNotification();
        updateAccount(mAccount);
        Assert.assertTrue("switching to account with different settings should notify",
                mSyncSettingsObserver.receivedNotification());

        mSyncSettingsObserver.clearNotification();
        AndroidSyncSettings.enableChromeSync(mContext);
        Assert.assertFalse("enableChromeSync shouldn't trigger observers",
                mSyncSettingsObserver.receivedNotification());

        mSyncSettingsObserver.clearNotification();
        AndroidSyncSettings.disableChromeSync(mContext);
        Assert.assertTrue("disableChromeSync should trigger observers",
                mSyncSettingsObserver.receivedNotification());

        mSyncSettingsObserver.clearNotification();
        AndroidSyncSettings.disableChromeSync(mContext);
        Assert.assertFalse("disableChromeSync shouldn't observers",
                mSyncSettingsObserver.receivedNotification());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testIsSyncableOnSigninAndNotOnSignout()
            throws InterruptedException, TimeoutException {
        Assert.assertEquals(1, mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority));

        int currentCalls = mCallbackHelper.getCallCount();
        updateAccountWithCallback(null, new Callback<Boolean>() {
            @Override
            public void onResult(Boolean result) {
                Assert.assertTrue(result);
                mCallbackHelper.notifyCalled();
            }
        });
        mCallbackHelper.waitForCallback(currentCalls, mNumberOfCallsToWait);

        Assert.assertEquals(0, mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority));
        updateAccount(mAccount);
        Assert.assertEquals(1, mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority));
    }

    /**
     * Regression test for crbug.com/475299.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSyncableIsAlwaysSetWhenEnablingSync() throws InterruptedException {
        // Setup bad state.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        mSyncContentResolverDelegate.setIsSyncable(mAccount, mAuthority, 1);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        mSyncContentResolverDelegate.setIsSyncable(mAccount, mAuthority, 0);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        Assert.assertTrue(mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority) == 0);
        Assert.assertTrue(mSyncContentResolverDelegate.getSyncAutomatically(mAccount, mAuthority));

        // Ensure bug is fixed.
        enableChromeSyncOnUiThread();
        Assert.assertEquals(1, mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority));
        // Should still be enabled.
        Assert.assertTrue(mSyncContentResolverDelegate.getSyncAutomatically(mAccount, mAuthority));
    }
}
