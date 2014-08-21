// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.app.Activity;
import android.test.suitebuilder.annotation.LargeTest;
import android.util.Log;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.identity.UniqueIdentificationGenerator;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.identity.UuidBasedUniqueIdentificationGenerator;
import org.chromium.chrome.shell.ChromeShellActivity;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.chrome.shell.sync.SyncController;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.sync.notifier.SyncStatusHelper;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.sync.test.util.MockAccountManager;
import org.chromium.sync.test.util.MockSyncContentResolverDelegate;

import java.util.concurrent.TimeoutException;

/**
 * Test suite for Sync.
 */
public class SyncTest extends ChromeShellTestBase {
    private static final String TAG = "SyncTest";

    private static final String FOREIGN_SESSION_TEST_MACHINE_ID =
            "DeleteForeignSessionTest_Machine_1";

    private SyncTestUtil.SyncTestContext mContext;
    private MockAccountManager mAccountManager;
    private SyncController mSyncController;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        clearAppData();

        // Mock out the account manager on the device.
        mContext = new SyncTestUtil.SyncTestContext(getInstrumentation().getTargetContext());
        mAccountManager = new MockAccountManager(mContext, getInstrumentation().getContext());
        AccountManagerHelper.overrideAccountManagerHelperForTests(mContext, mAccountManager);
        MockSyncContentResolverDelegate syncContentResolverDelegate =
                new MockSyncContentResolverDelegate();
        syncContentResolverDelegate.setMasterSyncAutomatically(true);
        SyncStatusHelper.overrideSyncStatusHelperForTests(mContext, syncContentResolverDelegate);
        // This call initializes the ChromeSigninController to use our test context.
        ChromeSigninController.get(mContext);
        startChromeBrowserProcessSync(getInstrumentation().getTargetContext());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSyncController = SyncController.get(mContext);
            }
        });
        FakeServerHelper.useFakeServer(getInstrumentation().getTargetContext());
    }

    @Override
    protected void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSyncController.stop();
                FakeServerHelper.deleteFakeServer();
            }
        });

        super.tearDown();
    }

    @LargeTest
    @Feature({"Sync"})
    public void testGetAboutSyncInfoYieldsValidData() throws Throwable {
        setupTestAccountAndSignInToSync(FOREIGN_SESSION_TEST_MACHINE_ID);

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
    public void testAboutSyncPageDisplaysCurrentSyncStatus() throws InterruptedException {
        setupTestAccountAndSignInToSync(FOREIGN_SESSION_TEST_MACHINE_ID);

        loadUrlWithSanitization("chrome://sync");
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
                            contentViewCore, "document.documentElement.innerHTML");
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
    public void testDisableAndEnableSync() throws InterruptedException {
        setupTestAccountAndSignInToSync(FOREIGN_SESSION_TEST_MACHINE_ID);
        Account account =
                AccountManagerHelper.createAccountFromName(SyncTestUtil.DEFAULT_TEST_ACCOUNT);

        // Disabling Android sync should turn Chrome sync engine off.
        SyncStatusHelper.get(mContext).disableAndroidSync(account);
        SyncTestUtil.verifySyncIsDisabled(mContext, account);

        // Enabling Android sync should turn Chrome sync engine on.
        SyncStatusHelper.get(mContext).enableAndroidSync(account);
        SyncTestUtil.ensureSyncInitialized(mContext);
        SyncTestUtil.verifySignedInWithAccount(mContext, account);
    }

    private void setupTestAccountAndSignInToSync(
            final String syncClientIdentifier)
            throws InterruptedException {
        Account defaultTestAccount = SyncTestUtil.setupTestAccountThatAcceptsAllAuthTokens(
                mAccountManager, SyncTestUtil.DEFAULT_TEST_ACCOUNT, SyncTestUtil.DEFAULT_PASSWORD);

        UniqueIdentificationGeneratorFactory.registerGenerator(
                UuidBasedUniqueIdentificationGenerator.GENERATOR_ID,
                new UniqueIdentificationGenerator() {
                    @Override
                    public String getUniqueId(String salt) {
                        return syncClientIdentifier;
                    }
                }, true);

        SyncTestUtil.verifySyncIsSignedOut(getActivity());

        final Activity activity = launchChromeShellWithBlankPage();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSyncController.signIn(activity, SyncTestUtil.DEFAULT_TEST_ACCOUNT);
            }
        });

        SyncTestUtil.verifySyncIsSignedIn(mContext, defaultTestAccount);
        assertTrue("Sync everything should be enabled",
                SyncTestUtil.isSyncEverythingEnabled(mContext));
    }

    private static ContentViewCore getContentViewCore(ChromeShellActivity activity) {
        return activity.getActiveContentViewCore();
    }
}
