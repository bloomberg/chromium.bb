// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.content.Context;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.identity.UniqueIdentificationGenerator;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.identity.UuidBasedUniqueIdentificationGenerator;
import org.chromium.chrome.browser.signin.AccountIdProvider;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.sync.test.util.MockAccountManager;
import org.chromium.sync.test.util.MockSyncContentResolverDelegate;

import java.util.Set;

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
    protected ProfileSyncService mProfileSyncService;
    protected MockSyncContentResolverDelegate mSyncContentResolver;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        assertTrue("Clearing app data failed.", clearAppData());
        Context targetContext = getInstrumentation().getTargetContext();
        mContext = new SyncTestUtil.SyncTestContext(targetContext);

        mapAccountNamesToIds();
        setUpMockAndroidSyncSettings();
        setUpMockAccountManager();

        // Initializes ChromeSigninController to use our test context.
        ChromeSigninController.get(mContext);

        startChromeBrowserProcessSync(targetContext);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSyncController = SyncController.get(mContext);
                // Ensure SyncController is registered with the new AndroidSyncSettings.
                AndroidSyncSettings.registerObserver(mContext, mSyncController);
                mFakeServerHelper = FakeServerHelper.get();
            }
        });
        FakeServerHelper.useFakeServer(targetContext);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mProfileSyncService = ProfileSyncService.get(mContext);
            }
        });

        // Start the activity by opening about:blank. This URL is ideal because it is not synced as
        // a typed URL. If another URL is used, it could interfere with test data. This call is in
        // this location so that it takes place before any other calls to getActivity(). If
        // getActivity() is called without any prior configuration, an undesired URL
        // (e.g., google.com) will be opened.
        launchChromeShellWithBlankPage();
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
                AccountIdProvider.setInstanceForTest(new AccountIdProvider() {
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
        mSyncContentResolver = new MockSyncContentResolverDelegate();
        mSyncContentResolver.setMasterSyncAutomatically(true);
        AndroidSyncSettings.overrideForTests(mContext, mSyncContentResolver);
    }

    protected Account setupTestAccount(final String syncClientIdentifier)
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
        return defaultTestAccount;
    }

    protected void setupTestAccountAndSignInToSync(
            final String syncClientIdentifier)
            throws InterruptedException {
        Account defaultTestAccount = setupTestAccount(syncClientIdentifier);
        signIn(defaultTestAccount);
        SyncTestUtil.verifySyncIsActiveForAccount(mContext, defaultTestAccount);
        assertTrue("Sync everything should be enabled",
                SyncTestUtil.isSyncEverythingEnabled(mContext));
    }

    protected void startSync() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SyncController.get(mContext).start();
            }
        });
        SyncTestUtil.waitForSyncActive(mContext);
    }

    protected void stopSync() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SyncController.get(mContext).stop();
            }
        });
        getInstrumentation().waitForIdleSync();
    }

    protected void signIn(final Account account) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSyncController.signIn(getActivity(), account.name);
            }
        });
    }

    protected void signOut() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SigninManager.get(mContext).signOut(getActivity(), null);
            }
        });
    }

    protected void disableDataType(final int modelType) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Set<Integer> preferredTypes = mProfileSyncService.getPreferredDataTypes();
                preferredTypes.remove(modelType);
                mProfileSyncService.setPreferredDataTypes(false, preferredTypes);
            }
        });
    }
}
