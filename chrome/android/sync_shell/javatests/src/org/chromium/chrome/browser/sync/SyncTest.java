// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.app.Activity;
import android.test.suitebuilder.annotation.LargeTest;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.sync.AndroidSyncSettings;

/**
 * Test suite for Sync.
 */
public class SyncTest extends SyncTestBase {
    private static final String TAG = "SyncTest";

    @LargeTest
    @Feature({"Sync"})
    public void testFlushDirectoryDoesntBreakSync() throws Throwable {
        setUpTestAccountAndSignInToSync();
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
        Account account = setUpTestAccountAndSignInToSync();

        // Signing out should disable sync.
        signOut();
        SyncTestUtil.verifySyncIsSignedOut(mContext);

        // Signing back in should re-enable sync.
        signIn(account);
        SyncTestUtil.verifySyncIsActiveForAccount(mContext, account);
    }

    @LargeTest
    @Feature({"Sync"})
    public void testStopAndStartSync() throws InterruptedException {
        Account account = setUpTestAccountAndSignInToSync();

        SyncTestUtil.verifySyncIsActiveForAccount(mContext, account);
        stopSync();
        SyncTestUtil.verifySyncIsDisabled(mContext, account);
        startSync();
        SyncTestUtil.verifySyncIsActiveForAccount(mContext, account);
    }

    @LargeTest
    @Feature({"Sync"})
    public void testDisableAndEnableSyncThroughAndroid() throws InterruptedException {
        Account account = setUpTestAccountAndSignInToSync();
        SyncTestUtil.waitForSyncActive();

        String authority = AndroidSyncSettings.getContractAuthority(mContext);

        // Disabling Android sync should turn Chrome sync engine off.
        mSyncContentResolver.setSyncAutomatically(account, authority, false);
        SyncTestUtil.verifySyncIsDisabled(mContext, account);

        // Enabling Android sync should turn Chrome sync engine on.
        mSyncContentResolver.setSyncAutomatically(account, authority, true);
        SyncTestUtil.verifySyncIsActiveForAccount(mContext, account);
    }

    private static ContentViewCore getContentViewCore(ChromeActivity activity) {
        return activity.getActivityTab().getContentViewCore();
    }
}
