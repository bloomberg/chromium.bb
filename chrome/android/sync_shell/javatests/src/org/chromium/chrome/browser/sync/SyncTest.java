// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.app.Activity;
import android.support.test.filters.LargeTest;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.signin.AccountIdProvider;
import org.chromium.chrome.browser.signin.AccountTrackerService;
import org.chromium.chrome.browser.signin.SigninHelper;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.test.util.browser.signin.MockChangeEventChecker;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Test suite for Sync.
 */
@RetryOnFailure  // crbug.com/637448
public class SyncTest extends SyncTestBase {
    private static final String TAG = "SyncTest";

    @LargeTest
    @Feature({"Sync"})
    public void testFlushDirectoryDoesntBreakSync() throws Throwable {
        setUpTestAccountAndSignIn();
        final Activity activity = getActivity();

        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                ApplicationStatus.onStateChangeForTesting(activity, ActivityState.PAUSED);
            }
        });

        // TODO(pvalenzuela): When available, check that sync is still functional.
    }

    @LargeTest
    @Feature({"Sync"})
    public void testSignInAndOut() throws InterruptedException {
        Account account = setUpTestAccountAndSignIn();

        // Signing out should disable sync.
        signOut();
        assertFalse(SyncTestUtil.isSyncRequested());

        // Signing back in should re-enable sync.
        signIn(account);
        SyncTestUtil.waitForSyncActive();
    }

    @LargeTest
    @Feature({"Sync"})
    public void testStopAndClear() {
        setUpTestAccountAndSignIn();
        CriteriaHelper.pollUiThread(
                new Criteria("Timed out checking that isSignedInOnNative() == true") {
                    @Override
                    public boolean isSatisfied() {
                        return SigninManager.get(mContext).isSignedInOnNative();
                    }
                },
                SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);

        clearServerData();

        // Clearing server data should turn off sync and sign out of chrome.
        assertNull(SigninTestUtil.getCurrentAccount());
        assertFalse(SyncTestUtil.isSyncRequested());
        CriteriaHelper.pollUiThread(
                new Criteria("Timed out checking that isSignedInOnNative() == false") {
                    @Override
                    public boolean isSatisfied() {
                        return !SigninManager.get(mContext).isSignedInOnNative();
                    }
                },
                SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);
    }

    /*
     * @FlakyTest
     * @LargeTest
     * @Feature({"Sync"})
     */
    @DisabledTest(message = "crbug.com/588050,crbug.com/595893")
    public void testRename() {
        // The two accounts object that would represent the account rename.
        final Account oldAccount = setUpTestAccountAndSignIn();
        final Account newAccount = SigninTestUtil.addTestAccount("test2@gmail.com");

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // First, we force a call to updateAccountRenameData. In the real world,
                // this should be called by one of our broadcast listener that listens to
                // real account rename events instead of the mocks.
                MockChangeEventChecker eventChecker = new MockChangeEventChecker();
                eventChecker.insertRenameEvent(oldAccount.name, newAccount.name);
                SigninHelper.resetAccountRenameEventIndex(mContext);
                SigninHelper.updateAccountRenameData(mContext, eventChecker);

                // Tell the fake content resolver that a rename had happen and copy over the sync
                // settings. This would normally be done by the SystemSyncContentResolver.
                mSyncContentResolver.renameAccounts(
                        oldAccount, newAccount, AndroidSyncSettings.getContractAuthority(mContext));

                // Inform the AccountTracker, these would normally be done by account validation
                // or signin. We will only be calling the testing versions of it.
                AccountIdProvider provider = AccountIdProvider.getInstance();
                String[] accountNames = {oldAccount.name, newAccount.name};
                String[] accountIds = {provider.getAccountId(mContext, accountNames[0]),
                                       provider.getAccountId(mContext, accountNames[1])};
                AccountTrackerService.get(mContext).syncForceRefreshForTest(
                        accountIds, accountNames);

                // Starts the rename process. Normally, this is triggered by the broadcast
                // listener as well.
                SigninHelper.get(mContext).validateAccountSettings(true);
            }
        });

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return newAccount.equals(SigninTestUtil.getCurrentAccount());
            }
        });
        SyncTestUtil.waitForSyncActive();
    }

    @LargeTest
    @Feature({"Sync"})
    public void testStopAndStartSync() {
        Account account = setUpTestAccountAndSignIn();

        stopSync();
        assertEquals(account, SigninTestUtil.getCurrentAccount());
        assertFalse(SyncTestUtil.isSyncRequested());

        startSyncAndWait();
    }

    /*
     * @LargeTest
     * @Feature({"Sync"})
     */
    @FlakyTest(message = "crbug.com/594558")
    public void testStopAndStartSyncThroughAndroid() {
        Account account = setUpTestAccountAndSignIn();

        String authority = AndroidSyncSettings.getContractAuthority(mContext);

        // Disabling Android sync should turn Chrome sync engine off.
        mSyncContentResolver.setSyncAutomatically(account, authority, false);
        assertFalse(SyncTestUtil.isSyncRequested());

        // Enabling Android sync should turn Chrome sync engine on.
        mSyncContentResolver.setSyncAutomatically(account, authority, true);
        SyncTestUtil.waitForSyncActive();

        // Disabling Android's master sync should turn Chrome sync engine off.
        mSyncContentResolver.setMasterSyncAutomatically(false);
        assertFalse(SyncTestUtil.isSyncRequested());

        // Enabling Android's master sync should turn Chrome sync engine on.
        mSyncContentResolver.setMasterSyncAutomatically(true);
        SyncTestUtil.waitForSyncActive();

        // Disabling both should definitely turn sync off.
        mSyncContentResolver.setSyncAutomatically(account, authority, false);
        mSyncContentResolver.setMasterSyncAutomatically(false);
        assertFalse(SyncTestUtil.isSyncRequested());

        // Re-enabling master sync should not turn sync back on.
        mSyncContentResolver.setMasterSyncAutomatically(true);
        assertFalse(SyncTestUtil.isSyncRequested());

        // But then re-enabling Chrome sync should.
        mSyncContentResolver.setSyncAutomatically(account, authority, true);
        SyncTestUtil.waitForSyncActive();
    }

    @LargeTest
    @Feature({"Sync"})
    public void testMasterSyncBlocksSyncStart() {
        setUpTestAccountAndSignIn();
        stopSync();
        assertFalse(SyncTestUtil.isSyncRequested());

        mSyncContentResolver.setMasterSyncAutomatically(false);
        startSync();
        assertFalse(SyncTestUtil.isSyncRequested());
    }

    private static ContentViewCore getContentViewCore(ChromeActivity activity) {
        return activity.getActivityTab().getContentViewCore();
    }
}
