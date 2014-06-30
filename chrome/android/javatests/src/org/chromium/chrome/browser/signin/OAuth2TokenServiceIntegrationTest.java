// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.sync.test.util.AccountHolder;
import org.chromium.sync.test.util.MockAccountManager;

import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration test for the OAuth2TokenService.
 *
 * These tests initialize the native part of the service.
 */
public class OAuth2TokenServiceIntegrationTest extends ChromeShellTestBase {

    private static final Account TEST_ACCOUNT1 =
            AccountManagerHelper.createAccountFromName("foo@gmail.com");
    private static final Account TEST_ACCOUNT2 =
            AccountManagerHelper.createAccountFromName("bar@gmail.com");
    private static final AccountHolder TEST_ACCOUNT_HOLDER_1 =
            AccountHolder.create().account(TEST_ACCOUNT1).build();
    private static final AccountHolder TEST_ACCOUNT_HOLDER_2 =
            AccountHolder.create().account(TEST_ACCOUNT2).build();

    private AdvancedMockContext mContext;
    private OAuth2TokenService mOAuth2TokenService;
    private MockAccountManager mAccountManager;
    private TestObserver mObserver;
    private ChromeSigninController mChromeSigninController;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        clearAppData();
        startChromeBrowserProcessSync(getInstrumentation().getTargetContext());

        // Set up AccountManager.
        mContext = new AdvancedMockContext(getInstrumentation().getTargetContext());
        mAccountManager = new MockAccountManager(mContext, getInstrumentation().getContext());
        AccountManagerHelper.overrideAccountManagerHelperForTests(mContext, mAccountManager);

        // Make sure there is no account signed in yet.
        mChromeSigninController = ChromeSigninController.get(mContext);
        mChromeSigninController.setSignedInAccountName(null);

        // Get a reference to the service.
        mOAuth2TokenService = getOAuth2TokenServiceOnUiThread();

        // Set up observer.
        mObserver = new TestObserver();
        addObserver(mObserver);
    }

    /**
     * The {@link OAuth2TokenService} and the {@link Profile} can only be accessed from the UI
     * thread, so this helper method is a convenience method to retrieve it.
     *
     * @return the OAuth2TokenService.
     */
    private static OAuth2TokenService getOAuth2TokenServiceOnUiThread() {
        final AtomicReference<OAuth2TokenService> service =
                new AtomicReference<OAuth2TokenService>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                service.set(OAuth2TokenService.getForProfile(Profile.getLastUsedProfile()));
            }
        });
        return service.get();
    }

    private void addObserver(final TestObserver observer) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mOAuth2TokenService.addObserver(observer);
            }
        });
    }

    @MediumTest
    @UiThreadTest
    @Feature({"Sync"})
    public void testFireRefreshTokenAvailableNotifiesJavaObservers() {
        // Adding an observer should not lead to a callback.
        assertEquals(0, mObserver.getAvailableCallCount());

        // An observer should be called with the correct account.
        mOAuth2TokenService.fireRefreshTokenAvailable(TEST_ACCOUNT1);
        assertEquals(1, mObserver.getAvailableCallCount());
        assertEquals(TEST_ACCOUNT1, mObserver.getLastAccount());

        // When mOAuth2TokenService, an observer should not be called.
        mOAuth2TokenService.removeObserver(mObserver);
        mOAuth2TokenService.fireRefreshTokenAvailable(TEST_ACCOUNT1);
        assertEquals(1, mObserver.getAvailableCallCount());

        // No other observer interface method should ever have been called.
        assertEquals(0, mObserver.getRevokedCallCount());
        assertEquals(0, mObserver.getLoadedCallCount());
    }

    @MediumTest
    @UiThreadTest
    @Feature({"Sync"})
    public void testFireRefreshTokenRevokedNotifiesJavaObservers() {
        // Adding an observer should not lead to a callback.
        assertEquals(0, mObserver.getRevokedCallCount());

        // An observer should be called with the correct account.
        mOAuth2TokenService.fireRefreshTokenRevoked(TEST_ACCOUNT1);
        assertEquals(1, mObserver.getRevokedCallCount());
        assertEquals(TEST_ACCOUNT1, mObserver.getLastAccount());

        // When removed, an observer should not be called.
        mOAuth2TokenService.removeObserver(mObserver);
        mOAuth2TokenService.fireRefreshTokenRevoked(TEST_ACCOUNT1);
        assertEquals(1, mObserver.getRevokedCallCount());

        // No other observer interface method should ever have been called.
        assertEquals(0, mObserver.getAvailableCallCount());
        assertEquals(0, mObserver.getLoadedCallCount());
    }

    @MediumTest
    @UiThreadTest
    @Feature({"Sync"})
    public void testFireRefreshTokensLoadedNotifiesJavaObservers() {
        // Adding an observer should not lead to a callback.
        assertEquals(0, mObserver.getLoadedCallCount());

        // An observer should be called with the correct account.
        mOAuth2TokenService.fireRefreshTokensLoaded();
        assertEquals(1, mObserver.getLoadedCallCount());

        // When removed, an observer should not be called.
        mOAuth2TokenService.removeObserver(mObserver);
        mOAuth2TokenService.fireRefreshTokensLoaded();
        assertEquals(1, mObserver.getLoadedCallCount());

        // No other observer interface method should ever have been called.
        assertEquals(0, mObserver.getAvailableCallCount());
        assertEquals(0, mObserver.getRevokedCallCount());
    }

    @MediumTest
    @UiThreadTest
    public void testValidateAccountsNoAccountsRegisteredAndNoSignedInUser() {
        // Run test.
        mOAuth2TokenService.validateAccounts(mContext, false);

        // Ensure no calls have been made to the observer.
        assertEquals(0, mObserver.getAvailableCallCount());
        assertEquals(0, mObserver.getRevokedCallCount());
        assertEquals(0, mObserver.getLoadedCallCount());
    }

    @MediumTest
    @UiThreadTest
    public void testValidateAccountsOneAccountsRegisteredAndNoSignedInUser() {
        // Add account.
        mAccountManager.addAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_1);

        // Run test.
        mOAuth2TokenService.validateAccounts(mContext, false);

        // Ensure no calls have been made to the observer.
        assertEquals(0, mObserver.getAvailableCallCount());
        assertEquals(0, mObserver.getRevokedCallCount());
        assertEquals(0, mObserver.getLoadedCallCount());
    }

    @MediumTest
    @UiThreadTest
    public void testValidateAccountsOneAccountsRegisteredSignedIn() {
        // Add account.
        mAccountManager.addAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_1);

        // Mark user as signed in.
        mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

        // Run test.
        mOAuth2TokenService.validateAccounts(mContext, false);

        // Ensure one call for the signed in account.
        assertEquals(1, mObserver.getAvailableCallCount());
        assertEquals(0, mObserver.getRevokedCallCount());
        assertEquals(0, mObserver.getLoadedCallCount());

        // Validate again and make sure no new calls are made.
        mOAuth2TokenService.validateAccounts(mContext, false);
        assertEquals(1, mObserver.getAvailableCallCount());
        assertEquals(0, mObserver.getRevokedCallCount());
        assertEquals(0, mObserver.getLoadedCallCount());

        // Validate again with force notifications and make sure one new calls is made.
        mOAuth2TokenService.validateAccounts(mContext, true);
        assertEquals(2, mObserver.getAvailableCallCount());
        assertEquals(0, mObserver.getRevokedCallCount());
        assertEquals(0, mObserver.getLoadedCallCount());
    }

    @MediumTest
    @UiThreadTest
    public void testValidateAccountsSingleAccountWithoutChanges() {
        // Add account.
        mAccountManager.addAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_1);

        // Mark user as signed in.
        mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

        // Run one validation.
        mOAuth2TokenService.validateAccounts(mContext, false);
        assertEquals(1, mObserver.getAvailableCallCount());
        assertEquals(0, mObserver.getRevokedCallCount());
        assertEquals(0, mObserver.getLoadedCallCount());

        // Re-run validation.
        mOAuth2TokenService.validateAccounts(mContext, false);
        assertEquals(1, mObserver.getAvailableCallCount());
        assertEquals(0, mObserver.getRevokedCallCount());
        assertEquals(0, mObserver.getLoadedCallCount());
    }

    @MediumTest
    @UiThreadTest
    public void testValidateAccountsSingleAccountThenAddOne() {
        // Add account.
        mAccountManager.addAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_1);

        // Mark user as signed in.
        mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

        // Run one validation.
        mOAuth2TokenService.validateAccounts(mContext, false);
        assertEquals(1, mObserver.getAvailableCallCount());
        assertEquals(0, mObserver.getRevokedCallCount());
        assertEquals(0, mObserver.getLoadedCallCount());

        // Add another account.
        mAccountManager.addAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_2);

        // Re-run validation.
        mOAuth2TokenService.validateAccounts(mContext, false);
        assertEquals(2, mObserver.getAvailableCallCount());
        assertEquals(0, mObserver.getRevokedCallCount());
        assertEquals(0, mObserver.getLoadedCallCount());
    }

    @MediumTest
    @UiThreadTest
    public void testValidateAccountsTwoAccountsThenRemoveOne() {
        // Add accounts.
        mAccountManager.addAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_1);
        mAccountManager.addAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_2);

        // Mark user as signed in.
        mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

        // Run one validation.
        mOAuth2TokenService.validateAccounts(mContext, false);
        assertEquals(2, mObserver.getAvailableCallCount());

        mAccountManager.removeAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_2);
        mOAuth2TokenService.validateAccounts(mContext, false);

        assertEquals(2, mObserver.getAvailableCallCount());
        assertEquals(1, mObserver.getRevokedCallCount());
        assertEquals(0, mObserver.getLoadedCallCount());
    }

    @MediumTest
    @UiThreadTest
    public void testValidateAccountsTwoAccountsThenRemoveAll() {
        // Add accounts.
        mAccountManager.addAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_1);
        mAccountManager.addAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_2);

        // Mark user as signed in.
        mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

        mOAuth2TokenService.validateAccounts(mContext, false);
        assertEquals(2, mObserver.getAvailableCallCount());

        // Remove all.
        mAccountManager.removeAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_1);
        mAccountManager.removeAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_2);

        // Re-validate and run checks.
        mOAuth2TokenService.validateAccounts(mContext, false);
        assertEquals(2, mObserver.getRevokedCallCount());
        assertEquals(0, mObserver.getLoadedCallCount());
    }

    @MediumTest
    @UiThreadTest
    public void testValidateAccountsTwoAccountsThenRemoveAllSignOut() {
        // Add accounts.
        mAccountManager.addAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_1);
        mAccountManager.addAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_2);

        // Mark user as signed in.
        mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

        mOAuth2TokenService.validateAccounts(mContext, false);
        assertEquals(2, mObserver.getAvailableCallCount());

        // Remove all.
        mChromeSigninController.clearSignedInUser();
        mAccountManager.removeAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_1);
        mAccountManager.removeAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_2);

        // Re-validate and run checks.
        mOAuth2TokenService.validateAccounts(mContext, false);
        assertEquals(2, mObserver.getRevokedCallCount());
        assertEquals(0, mObserver.getLoadedCallCount());
    }

    @MediumTest
    @UiThreadTest
    public void testValidateAccountsTwoAccountsRegisteredAndOneSignedIn() {
        // Add accounts.
        mAccountManager.addAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_1);
        mAccountManager.addAccountHolderExplicitly(TEST_ACCOUNT_HOLDER_2);

        // Mark user as signed in.
        mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

        // Run test.
        mOAuth2TokenService.validateAccounts(mContext, false);

        // All accounts will be notified. It is up to the observer
        // to design if any action is needed.
        assertEquals(2, mObserver.getAvailableCallCount());
        assertEquals(0, mObserver.getRevokedCallCount());
        assertEquals(0, mObserver.getLoadedCallCount());
    }

    @MediumTest
    @UiThreadTest
    public void testValidateAccountsNoAccountsRegisteredButSignedIn() {
        // Mark user as signed in without setting up the account.
        mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

        // Run test.
        mOAuth2TokenService.validateAccounts(mContext, false);

        // Ensure no calls have been made to the observer.
        assertEquals(0, mObserver.getAvailableCallCount());
        assertEquals(0, mObserver.getRevokedCallCount());
        assertEquals(0, mObserver.getLoadedCallCount());
    }

    @MediumTest
    @UiThreadTest
    public void testValidateAccountsFiresEventAtTheEnd() {
        // Mark user as signed in without setting up the account.
        mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);
        TestObserver ob = new TestObserver() {
            @Override
            public void onRefreshTokenAvailable(Account account) {
                super.onRefreshTokenAvailable(account);
                assertEquals(1, OAuth2TokenService.getAccounts(mContext).length);
            }
        };

        addObserver(ob);
        mOAuth2TokenService.validateAccounts(mContext, false);
    }

    private static class TestObserver implements OAuth2TokenService.OAuth2TokenServiceObserver {
        private int mAvailableCallCount;
        private int mRevokedCallCount;
        private int mLoadedCallCount;
        private Account mLastAccount;

        @Override
        public void onRefreshTokenAvailable(Account account) {
            mAvailableCallCount++;
            mLastAccount = account;
        }

        @Override
        public void onRefreshTokenRevoked(Account account) {
            mRevokedCallCount++;
            mLastAccount = account;
        }

        @Override
        public void onRefreshTokensLoaded() {
            mLoadedCallCount++;
        }

        public int getAvailableCallCount() {
            return mAvailableCallCount;
        }

        public int getRevokedCallCount() {
            return mRevokedCallCount;
        }

        public int getLoadedCallCount() {
            return mLoadedCallCount;
        }

        public Account getLastAccount() {
            return mLastAccount;
        }
    }
}
