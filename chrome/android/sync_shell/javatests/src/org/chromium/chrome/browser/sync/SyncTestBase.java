// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.content.Context;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.identity.UniqueIdentificationGenerator;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.identity.UuidBasedUniqueIdentificationGenerator;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.ModelType;
import org.chromium.sync.test.util.MockSyncContentResolverDelegate;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Base class for common functionality between sync tests.
 */
public class SyncTestBase extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String TAG = "SyncTestBase";

    private static final String CLIENT_ID = "Client_ID";

    private static final Set<Integer> USER_SELECTABLE_TYPES =
            new HashSet<Integer>(Arrays.asList(new Integer[] {
                ModelType.AUTOFILL,
                ModelType.BOOKMARKS,
                ModelType.PASSWORDS,
                ModelType.PREFERENCES,
                ModelType.PROXY_TABS,
                ModelType.TYPED_URLS,
            }));

    protected Context mContext;
    protected SyncController mSyncController;
    protected FakeServerHelper mFakeServerHelper;
    protected ProfileSyncService mProfileSyncService;
    protected MockSyncContentResolverDelegate mSyncContentResolver;

    public SyncTestBase() {
      super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        // Start the activity by opening about:blank. This URL is ideal because it is not synced as
        // a typed URL. If another URL is used, it could interfere with test data.
        startMainActivityOnBlankPage();
    }

    @Override
    protected void setUp() throws Exception {
        // This must be called before super.setUp() in order for test authentication to work.
        SigninTestUtil.setUpAuthForTest(getInstrumentation());

        super.setUp();
        mContext = getInstrumentation().getTargetContext();

        setUpMockAndroidSyncSettings();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSyncController = SyncController.get(mContext);
                // Ensure SyncController is registered with the new AndroidSyncSettings.
                AndroidSyncSettings.registerObserver(mContext, mSyncController);
                mFakeServerHelper = FakeServerHelper.get();
            }
        });
        FakeServerHelper.useFakeServer(mContext);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mProfileSyncService = ProfileSyncService.get();
            }
        });

        UniqueIdentificationGeneratorFactory.registerGenerator(
                UuidBasedUniqueIdentificationGenerator.GENERATOR_ID,
                new UniqueIdentificationGenerator() {
                    @Override
                    public String getUniqueId(String salt) {
                        return CLIENT_ID;
                    }
                }, true);
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
        SigninTestUtil.get().resetSigninState();

        super.tearDown();
    }

    private void setUpMockAndroidSyncSettings() {
        mSyncContentResolver = new MockSyncContentResolverDelegate();
        mSyncContentResolver.setMasterSyncAutomatically(true);
        AndroidSyncSettings.overrideForTests(mContext, mSyncContentResolver);
    }

    protected Account setUpTestAccount() throws InterruptedException {
        Account account = SigninTestUtil.get().addAndSignInTestAccount();
        SyncTestUtil.verifySyncIsSignedOut(getActivity());
        return account;
    }

    protected Account setUpTestAccountAndSignInToSync() throws InterruptedException {
        Account account = setUpTestAccount();
        signIn(account);
        SyncTestUtil.verifySyncIsActiveForAccount(mContext, account);
        assertTrue("Sync everything should be enabled",
                SyncTestUtil.isSyncEverythingEnabled(mContext));
        return account;
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
                preferredTypes.retainAll(USER_SELECTABLE_TYPES);
                preferredTypes.remove(modelType);
                mProfileSyncService.setPreferredDataTypes(false, preferredTypes);
            }
        });
    }
}
