// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.app.Activity;
import android.test.suitebuilder.annotation.LargeTest;
import android.util.Log;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.sync.AndroidSyncSettings;

import java.util.concurrent.TimeoutException;

/**
 * Test suite for Sync.
 */
public class SyncTest extends SyncTestBase {
    private static final String TAG = "SyncTest";

    @LargeTest
    @Feature({"Sync"})
    public void testGetAboutSyncInfoYieldsValidData() throws Throwable {
        setUpTestAccountAndSignInToSync();

        final SyncTestUtil.AboutSyncInfoGetter syncInfoGetter =
                new SyncTestUtil.AboutSyncInfoGetter(getActivity());
        runTestOnUiThread(syncInfoGetter);

        boolean gotInfo = CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !syncInfoGetter.getAboutInfo().isEmpty();
            }
        }, SyncTestUtil.UI_TIMEOUT_MS, SyncTestUtil.CHECK_INTERVAL_MS);

        assertTrue("Couldn't get about info.", gotInfo);
    }

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
    public void testAboutSyncPageDisplaysCurrentSyncStatus() throws InterruptedException {
        setUpTestAccountAndSignInToSync();

        loadUrl("chrome://sync");
        SyncTestUtil.AboutSyncInfoGetter aboutInfoGetter =
                new SyncTestUtil.AboutSyncInfoGetter(getActivity());
        try {
            runTestOnUiThread(aboutInfoGetter);
        } catch (Throwable t) {
            Log.w(TAG,
                    "Exception while trying to fetch about sync info from ProfileSyncService.", t);
            fail("Unable to fetch sync info from ProfileSyncService.");
        }
        assertFalse("About sync info should not be empty.",
                aboutInfoGetter.getAboutInfo().isEmpty());
        assertTrue("About sync info should have sync summary status.",
                aboutInfoGetter.getAboutInfo().containsKey(SyncTestUtil.SYNC_SUMMARY_STATUS));
        final String expectedSyncSummary =
                aboutInfoGetter.getAboutInfo().get(SyncTestUtil.SYNC_SUMMARY_STATUS);

        Criteria checker = new Criteria() {
            @Override
            public boolean isSatisfied() {
                final ContentViewCore contentViewCore = getContentViewCore(getActivity());
                String innerHtml = "";
                try {
                    innerHtml = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                            contentViewCore.getWebContents(), "document.documentElement.innerHTML");
                } catch (InterruptedException e) {
                    Log.w(TAG, "Interrupted while polling about:sync page for sync status.", e);
                } catch (TimeoutException e) {
                    Log.w(TAG, "Interrupted while polling about:sync page for sync status.", e);
                }
                return innerHtml.contains(expectedSyncSummary);
            }

        };
        boolean hadExpectedStatus = CriteriaHelper.pollForCriteria(
                checker, SyncTestUtil.UI_TIMEOUT_MS, SyncTestUtil.CHECK_INTERVAL_MS);
        assertTrue("Sync status not present on about sync page: " + expectedSyncSummary,
                hadExpectedStatus);
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
        SyncTestUtil.waitForSyncActive(mContext);

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
