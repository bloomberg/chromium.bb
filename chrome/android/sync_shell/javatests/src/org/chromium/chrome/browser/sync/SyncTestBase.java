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
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.test.util.MockSyncContentResolverDelegate;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

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

    protected abstract class DataCriteria<T> extends Criteria {
        public DataCriteria() {
            super("Sync data criteria not met.");
        }

        public abstract boolean isSatisfied(List<T> data);

        public abstract List<T> getData() throws Exception;

        @Override
        public boolean isSatisfied() {
            try {
                return isSatisfied(getData());
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }

    protected Context mContext;
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
                // Ensure SyncController is registered with the new AndroidSyncSettings.
                AndroidSyncSettings.registerObserver(mContext, SyncController.get(mContext));
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
                mProfileSyncService.requestStop();
                FakeServerHelper.deleteFakeServer();
            }
        });
        SigninTestUtil.resetSigninState();

        super.tearDown();
    }

    private void setUpMockAndroidSyncSettings() {
        mSyncContentResolver = new MockSyncContentResolverDelegate();
        mSyncContentResolver.setMasterSyncAutomatically(true);
        AndroidSyncSettings.overrideForTests(mContext, mSyncContentResolver);
    }

    protected Account setUpTestAccount() {
        Account account = SigninTestUtil.addTestAccount();
        assertFalse(SyncTestUtil.isSyncRequested());
        return account;
    }

    protected Account setUpTestAccountAndSignIn() {
        Account account = setUpTestAccount();
        signIn(account);
        return account;
    }

    protected void startSync() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mProfileSyncService.requestStart();
            }
        });
    }

    protected void startSyncAndWait() {
        startSync();
        SyncTestUtil.waitForSyncActive();
    }

    protected void stopSync() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mProfileSyncService.requestStop();
            }
        });
        getInstrumentation().waitForIdleSync();
    }

    protected void signIn(final Account account) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SigninManager.get(mContext).signIn(account, null, null);
            }
        });
        SyncTestUtil.waitForSyncActive();
        SyncTestUtil.triggerSyncAndWaitForCompletion();
        assertEquals(account, SigninTestUtil.getCurrentAccount());
    }

    protected void signOut() throws InterruptedException {
        final Semaphore s = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SigninManager.get(mContext).signOut(new Runnable() {
                    @Override
                    public void run() {
                        s.release();
                    }
                });
            }
        });
        assertTrue(s.tryAcquire(SyncTestUtil.TIMEOUT_MS, TimeUnit.MILLISECONDS));
        assertNull(SigninTestUtil.getCurrentAccount());
        assertFalse(SyncTestUtil.isSyncRequested());
    }

    protected void clearServerData() {
        mFakeServerHelper.clearServerData();
        SyncTestUtil.triggerSync();
        CriteriaHelper.pollUiThread(new Criteria("Timed out waiting for sync to stop.") {
            @Override
            public boolean isSatisfied() {
                return !ProfileSyncService.get().isSyncRequested();
            }
        }, SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);
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

    protected void pollInstrumentationThread(Criteria criteria) {
        CriteriaHelper.pollInstrumentationThread(
                criteria, SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);
    }
}
