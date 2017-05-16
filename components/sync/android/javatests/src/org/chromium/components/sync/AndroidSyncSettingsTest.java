// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync;

import android.accounts.Account;
import android.content.Context;
import android.os.Bundle;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.components.signin.AccountManagerHelper;
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
public class AndroidSyncSettingsTest extends InstrumentationTestCase {
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

    @Override
    protected void setUp() throws Exception {
        mContext = getInstrumentation().getTargetContext();
        setupTestAccounts(mContext);
        // Set signed in account to mAccount before initializing AndroidSyncSettings to let
        // AndroidSyncSettings establish correct assumptions.
        ChromeSigninController.get().setSignedInAccountName(mAccount.name);

        mSyncContentResolverDelegate = new CountingMockSyncContentResolverDelegate();
        AndroidSyncSettings.overrideForTests(mContext, mSyncContentResolverDelegate);
        mAuthority = AndroidSyncSettings.getContractAuthority(mContext);
        assertEquals(1, mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority));

        mSyncSettingsObserver = new MockSyncSettingsObserver();
        AndroidSyncSettings.registerObserver(mContext, mSyncSettingsObserver);

        super.setUp();
    }

    private void setupTestAccounts(Context context) {
        mAccountManager = new FakeAccountManagerDelegate(context);
        AccountManagerHelper.overrideAccountManagerHelperForTests(context, mAccountManager);
        mAccount = setupTestAccount("account@example.com");
        mAlternateAccount = setupTestAccount("alternate@example.com");
    }

    private Account setupTestAccount(String accountName) {
        Account account = AccountManagerHelper.createAccountFromName(accountName);
        AccountHolder.Builder accountHolder =
                AccountHolder.builder(account).password("password").alwaysAccept(true);
        mAccountManager.addAccountHolderExplicitly(accountHolder.build());
        return account;
    }

    @Override
    protected void tearDown() throws Exception {
        AccountManagerHelper.resetAccountManagerHelperForTests();
        super.tearDown();
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

    @SmallTest
    @Feature({"Sync"})
    public void testAccountInitialization() throws InterruptedException, TimeoutException {
        // mAccount was set to be syncable and not have periodic syncs.
        assertEquals(1, mSyncContentResolverDelegate.mSetIsSyncableCalls.get());
        assertEquals(1, mSyncContentResolverDelegate.mRemovePeriodicSyncCalls.get());

        final CallbackHelper callbackHelper = new CallbackHelper();
        AndroidSyncSettings.updateAccount(mContext, null, new Callback<Boolean>() {
            @Override
            public void onResult(Boolean result) {
                callbackHelper.notifyCalled();
            }
        });
        callbackHelper.waitForCallback(0);

        // mAccount was set to be not syncable.
        assertEquals(2, mSyncContentResolverDelegate.mSetIsSyncableCalls.get());
        assertEquals(1, mSyncContentResolverDelegate.mRemovePeriodicSyncCalls.get());
        AndroidSyncSettings.updateAccount(mContext, mAlternateAccount);
        // mAlternateAccount was set to be syncable and not have periodic syncs.
        assertEquals(3, mSyncContentResolverDelegate.mSetIsSyncableCalls.get());
        assertEquals(2, mSyncContentResolverDelegate.mRemovePeriodicSyncCalls.get());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testToggleMasterSyncFromSettings() throws InterruptedException {
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("master sync should be set", AndroidSyncSettings.isMasterSyncEnabled(mContext));

        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertFalse(
                "master sync should be unset", AndroidSyncSettings.isMasterSyncEnabled(mContext));
    }

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
        assertTrue("sync should be set", AndroidSyncSettings.isSyncEnabled(mContext));
        assertTrue("sync should be set for chrome app",
                AndroidSyncSettings.isChromeSyncEnabled(mContext));

        // Disable sync automatically for the app
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, false);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertFalse("sync should be unset", AndroidSyncSettings.isSyncEnabled(mContext));
        assertFalse("sync should be unset for chrome app",
                AndroidSyncSettings.isChromeSyncEnabled(mContext));

        // Re-enable sync
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("sync should be re-enabled", AndroidSyncSettings.isSyncEnabled(mContext));
        assertTrue("sync should be set for chrome app",
                AndroidSyncSettings.isChromeSyncEnabled(mContext));

        // Disabled from master sync
        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertFalse("sync should be disabled due to master sync",
                AndroidSyncSettings.isSyncEnabled(mContext));
        assertFalse("master sync should be disabled",
                AndroidSyncSettings.isMasterSyncEnabled(mContext));
        assertTrue("sync should be set for chrome app",
                AndroidSyncSettings.isChromeSyncEnabled(mContext));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testToggleAccountSyncFromApplication() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        enableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("account should be synced", AndroidSyncSettings.isSyncEnabled(mContext));

        disableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertFalse("account should not be synced", AndroidSyncSettings.isSyncEnabled(mContext));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testToggleSyncabilityForMultipleAccounts() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        enableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("account should be synced", AndroidSyncSettings.isSyncEnabled(mContext));

        AndroidSyncSettings.updateAccount(mContext, mAlternateAccount);
        enableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue(
                "alternate account should be synced", AndroidSyncSettings.isSyncEnabled(mContext));

        disableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertFalse("alternate account should not be synced",
                AndroidSyncSettings.isSyncEnabled(mContext));
        AndroidSyncSettings.updateAccount(mContext, mAccount);
        assertTrue("account should still be synced", AndroidSyncSettings.isSyncEnabled(mContext));

        // Ensure we don't erroneously re-use cached data.
        AndroidSyncSettings.updateAccount(mContext, null);
        assertFalse(
                "null account should not be synced", AndroidSyncSettings.isSyncEnabled(mContext));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testSyncSettingsCaching() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        enableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("account should be synced", AndroidSyncSettings.isSyncEnabled(mContext));

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
        assertEquals(masterSyncAutomaticallyCalls,
                mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls.get());
        assertEquals(isSyncableCalls, mSyncContentResolverDelegate.mGetIsSyncableCalls.get());
        assertEquals(getSyncAutomaticallyAcalls,
                mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls.get());

        // Do a bunch of reads for alternate account.
        AndroidSyncSettings.updateAccount(mContext, mAlternateAccount);
        AndroidSyncSettings.isMasterSyncEnabled(mContext);
        AndroidSyncSettings.isSyncEnabled(mContext);
        AndroidSyncSettings.isChromeSyncEnabled(mContext);

        // Ensure settings were only fetched once.
        assertEquals(masterSyncAutomaticallyCalls + 1,
                mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls.get());
        assertEquals(isSyncableCalls + 1, mSyncContentResolverDelegate.mGetIsSyncableCalls.get());
        assertEquals(getSyncAutomaticallyAcalls + 1,
                mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls.get());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testGetContractAuthority() throws Exception {
        assertEquals("The contract authority should be the package name.",
                getInstrumentation().getTargetContext().getPackageName(),
                AndroidSyncSettings.getContractAuthority(mContext));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testAndroidSyncSettingsPostsNotifications() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        mSyncSettingsObserver.clearNotification();
        AndroidSyncSettings.enableChromeSync(mContext);
        assertTrue("enableChromeSync should trigger observers",
                mSyncSettingsObserver.receivedNotification());

        mSyncSettingsObserver.clearNotification();
        AndroidSyncSettings.updateAccount(mContext, mAlternateAccount);
        assertTrue("switching to account with different settings should notify",
                mSyncSettingsObserver.receivedNotification());

        mSyncSettingsObserver.clearNotification();
        AndroidSyncSettings.updateAccount(mContext, mAccount);
        assertTrue("switching to account with different settings should notify",
                mSyncSettingsObserver.receivedNotification());

        mSyncSettingsObserver.clearNotification();
        AndroidSyncSettings.enableChromeSync(mContext);
        assertFalse("enableChromeSync shouldn't trigger observers",
                mSyncSettingsObserver.receivedNotification());

        mSyncSettingsObserver.clearNotification();
        AndroidSyncSettings.disableChromeSync(mContext);
        assertTrue("disableChromeSync should trigger observers",
                mSyncSettingsObserver.receivedNotification());

        mSyncSettingsObserver.clearNotification();
        AndroidSyncSettings.disableChromeSync(mContext);
        assertFalse("disableChromeSync shouldn't observers",
                mSyncSettingsObserver.receivedNotification());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testIsSyncableOnSigninAndNotOnSignout()
            throws InterruptedException, TimeoutException {
        assertEquals(1, mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority));

        final CallbackHelper callbackHelper = new CallbackHelper();
        AndroidSyncSettings.updateAccount(mContext, null, new Callback<Boolean>() {
            @Override
            public void onResult(Boolean result) {
                assertTrue(result);
                callbackHelper.notifyCalled();
            }
        });
        callbackHelper.waitForCallback(0);

        assertEquals(0, mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority));
        AndroidSyncSettings.updateAccount(mContext, mAccount);
        assertEquals(1, mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority));
    }

    /**
     * Regression test for crbug.com/475299.
     */
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
        assertTrue(mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority) == 0);
        assertTrue(mSyncContentResolverDelegate.getSyncAutomatically(mAccount, mAuthority));

        // Ensure bug is fixed.
        AndroidSyncSettings.enableChromeSync(mContext);
        assertEquals(1, mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority));
        // Should still be enabled.
        assertTrue(mSyncContentResolverDelegate.getSyncAutomatically(mAccount, mAuthority));
    }
}
