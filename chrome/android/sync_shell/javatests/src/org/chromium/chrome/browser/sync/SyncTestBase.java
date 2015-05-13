// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.identity.UniqueIdentificationGenerator;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.identity.UuidBasedUniqueIdentificationGenerator;
import org.chromium.chrome.browser.signin.AccountIdProvider;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.sync.test.util.MockAccountManager;
import org.chromium.sync.test.util.MockSyncContentResolverDelegate;

/**
 * Base class for common functionality between sync tests.
 */
public class SyncTestBase extends ChromeShellTestBase {
    private static final String TAG = "SyncTestBase";

    protected static final String CLIENT_ID = "Client_ID";

    protected SyncTestUtil.SyncTestContext mContext;
    protected MockAccountManager mAccountManager;
    protected SyncController mSyncController;
    protected FakeServerHelper mFakeServerHelper;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        clearAppData();
        Context targetContext = getInstrumentation().getTargetContext();
        mContext = new SyncTestUtil.SyncTestContext(targetContext);

        mapAccountNamesToIds();
        setUpMockAccountManager();
        setUpMockAndroidSyncSettings();

        // Initializes ChromeSigninController to use our test context.
        ChromeSigninController.get(mContext);

        startChromeBrowserProcessSync(targetContext);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSyncController = SyncController.get(mContext);
                mFakeServerHelper = FakeServerHelper.get();
            }
        });
        FakeServerHelper.useFakeServer(targetContext);
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

    private void mapAccountNamesToIds() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                if (AccountIdProvider.getInstance() != null) {
                    return;
                }

                AccountIdProvider.setInstance(new AccountIdProvider() {
                    @Override
                    public String getAccountId(Context ctx, String accountName) {
                        return "gaia-id-" + accountName;
                    }
                });
            }
        });
    }

    private void setUpMockAccountManager() {
        mAccountManager = new MockAccountManager(mContext, getInstrumentation().getContext());
        AccountManagerHelper.overrideAccountManagerHelperForTests(mContext, mAccountManager);
    }

    private void setUpMockAndroidSyncSettings() {
        MockSyncContentResolverDelegate syncContentResolverDelegate =
                new MockSyncContentResolverDelegate();
        syncContentResolverDelegate.setMasterSyncAutomatically(true);
        AndroidSyncSettings.overrideForTests(mContext, syncContentResolverDelegate);
    }

    protected void setupTestAccountAndSignInToSync(
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
}
