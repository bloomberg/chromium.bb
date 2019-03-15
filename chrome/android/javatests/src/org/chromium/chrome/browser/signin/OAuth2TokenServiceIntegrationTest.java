// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.test.util.ApplicationData;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.signin.AccountIdProvider;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.OAuth2TokenService;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;

/**
 * Integration test for the OAuth2TokenService.
 *
 * These tests initialize the native part of the service.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class OAuth2TokenServiceIntegrationTest {
    @Rule
    public NativeLibraryTestRule mActivityTestRule = new NativeLibraryTestRule();

    private static final Account TEST_ACCOUNT1 =
            AccountManagerFacade.createAccountFromName("foo@gmail.com");
    private static final Account TEST_ACCOUNT2 =
            AccountManagerFacade.createAccountFromName("bar@gmail.com");
    private static final AccountHolder TEST_ACCOUNT_HOLDER_1 =
            AccountHolder.builder(TEST_ACCOUNT1).alwaysAccept(true).build();
    private static final AccountHolder TEST_ACCOUNT_HOLDER_2 =
            AccountHolder.builder(TEST_ACCOUNT2).alwaysAccept(true).build();

    private OAuth2TokenService mOAuth2TokenService;
    private FakeAccountManagerDelegate mAccountManager;
    private TestObserver mObserver;
    private ChromeSigninController mChromeSigninController;

    @Before
    public void setUp() {
        mapAccountNamesToIds();
        ApplicationData.clearAppData(InstrumentationRegistry.getTargetContext());

        // loadNativeLibraryAndInitBrowserProcess will access AccountManagerFacade, so it should
        // be initialized beforehand.
        mAccountManager = new FakeAccountManagerDelegate(
                FakeAccountManagerDelegate.DISABLE_PROFILE_DATA_SOURCE);
        AccountManagerFacade.overrideAccountManagerFacadeForTests(mAccountManager);

        mActivityTestRule.loadNativeLibraryAndInitBrowserProcess();

        // Make sure there is no account signed in yet.
        mChromeSigninController = ChromeSigninController.get();
        mChromeSigninController.setSignedInAccountName(null);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Seed test accounts to AccountTrackerService.
            seedAccountTrackerService();

            // Get a reference to the service.
            mOAuth2TokenService = IdentityServicesProvider.getOAuth2TokenService();

            // Set up observer.
            mObserver = new TestObserver();
            mOAuth2TokenService.addObserver(mObserver);
        });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mChromeSigninController.setSignedInAccountName(null);
            mOAuth2TokenService.updateAccountList();
        });
        SigninHelper.resetSharedPrefs();
        SigninTestUtil.resetSigninState();
    }

    private void mapAccountNamesToIds() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            AccountIdProvider.setInstanceForTest(new AccountIdProvider() {
                @Override
                public String getAccountId(String accountName) {
                    return "gaia-id-" + accountName;
                }

                @Override
                public boolean canBeUsed() {
                    return true;
                }
            });
        });
    }

    private void seedAccountTrackerService() {
        AccountIdProvider provider = AccountIdProvider.getInstance();
        String[] accountNames = {TEST_ACCOUNT1.name, TEST_ACCOUNT2.name};
        String[] accountIds = {
                provider.getAccountId(accountNames[0]), provider.getAccountId(accountNames[1])};
        IdentityServicesProvider.getAccountTrackerService().syncForceRefreshForTest(
                accountIds, accountNames);
    }

    private void addAccount(AccountHolder accountHolder) {
        mAccountManager.addAccountHolderBlocking(accountHolder);
        ThreadUtils.runOnUiThreadBlocking(this::seedAccountTrackerService);
    }

    private void removeAccount(AccountHolder accountHolder) {
        mAccountManager.removeAccountHolderBlocking(accountHolder);
        ThreadUtils.runOnUiThreadBlocking(this::seedAccountTrackerService);
    }

    @Test
    @MediumTest
    public void testUpdateAccountListNoAccountsRegisteredAndNoSignedInUser() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Run test.
            mOAuth2TokenService.updateAccountList();

            // Ensure no calls have been made to the observer.
            Assert.assertEquals(0, mObserver.getAvailableCallCount());
            Assert.assertEquals(0, mObserver.getRevokedCallCount());
            Assert.assertEquals(0, mObserver.getLoadedCallCount());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListOneAccountsRegisteredAndNoSignedInUser() {
        addAccount(TEST_ACCOUNT_HOLDER_1);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Run test.
            mOAuth2TokenService.updateAccountList();

            // Ensure no calls have been made to the observer.
            Assert.assertEquals(0, mObserver.getAvailableCallCount());
            Assert.assertEquals(0, mObserver.getRevokedCallCount());
            Assert.assertEquals(0, mObserver.getLoadedCallCount());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListOneAccountsRegisteredSignedIn() {
        addAccount(TEST_ACCOUNT_HOLDER_1);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Mark user as signed in.
            mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

            // Run test.
            mOAuth2TokenService.updateAccountList();

            // Ensure one call for the signed in account.
            Assert.assertEquals(1, mObserver.getAvailableCallCount());
            Assert.assertEquals(0, mObserver.getRevokedCallCount());
            Assert.assertEquals(0, mObserver.getLoadedCallCount());

            // Validate again and make sure one new call is made.
            mOAuth2TokenService.updateAccountList();
            Assert.assertEquals(2, mObserver.getAvailableCallCount());
            Assert.assertEquals(0, mObserver.getRevokedCallCount());
            Assert.assertEquals(0, mObserver.getLoadedCallCount());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListSingleAccountThenAddOne() {
        addAccount(TEST_ACCOUNT_HOLDER_1);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Mark user as signed in.
            mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

            // Run one validation.
            mOAuth2TokenService.updateAccountList();
            Assert.assertEquals(1, mObserver.getAvailableCallCount());
            Assert.assertEquals(0, mObserver.getRevokedCallCount());
            Assert.assertEquals(0, mObserver.getLoadedCallCount());
        });

        // Add another account.
        addAccount(TEST_ACCOUNT_HOLDER_2);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Re-run validation.
            mOAuth2TokenService.updateAccountList();
            Assert.assertEquals(3, mObserver.getAvailableCallCount());
            Assert.assertEquals(0, mObserver.getRevokedCallCount());
            Assert.assertEquals(0, mObserver.getLoadedCallCount());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsThenRemoveOne() {
        // Add accounts.
        addAccount(TEST_ACCOUNT_HOLDER_1);
        addAccount(TEST_ACCOUNT_HOLDER_2);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Mark user as signed in.
            mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

            // Run one validation.
            mOAuth2TokenService.updateAccountList();
            Assert.assertEquals(2, mObserver.getAvailableCallCount());
            Assert.assertEquals(0, mObserver.getRevokedCallCount());
            Assert.assertEquals(0, mObserver.getLoadedCallCount());
        });

        removeAccount(TEST_ACCOUNT_HOLDER_2);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mOAuth2TokenService.updateAccountList();

            Assert.assertEquals(3, mObserver.getAvailableCallCount());
            Assert.assertEquals(1, mObserver.getRevokedCallCount());
            Assert.assertEquals(0, mObserver.getLoadedCallCount());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsThenRemoveAll() {
        // Add accounts.
        addAccount(TEST_ACCOUNT_HOLDER_1);
        addAccount(TEST_ACCOUNT_HOLDER_2);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Mark user as signed in.
            mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

            mOAuth2TokenService.updateAccountList();
            Assert.assertEquals(2, mObserver.getAvailableCallCount());
            Assert.assertEquals(0, mObserver.getRevokedCallCount());
            Assert.assertEquals(0, mObserver.getLoadedCallCount());
        });

        // Remove all.
        removeAccount(TEST_ACCOUNT_HOLDER_1);
        removeAccount(TEST_ACCOUNT_HOLDER_2);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Re-validate and run checks.
            mOAuth2TokenService.updateAccountList();
            Assert.assertEquals(2, mObserver.getAvailableCallCount());
            Assert.assertEquals(2, mObserver.getRevokedCallCount());
            Assert.assertEquals(0, mObserver.getLoadedCallCount());
        });
    }

    @Test
    @MediumTest
    @RetryOnFailure
    public void testUpdateAccountListTwoAccountsThenRemoveAllSignOut() {
        // Add accounts.
        addAccount(TEST_ACCOUNT_HOLDER_1);
        addAccount(TEST_ACCOUNT_HOLDER_2);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Mark user as signed in.
            mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

            mOAuth2TokenService.updateAccountList();
            Assert.assertEquals(2, mObserver.getAvailableCallCount());
            Assert.assertEquals(0, mObserver.getRevokedCallCount());
            Assert.assertEquals(0, mObserver.getLoadedCallCount());

            // Remove all.
            mChromeSigninController.setSignedInAccountName(null);
        });

        removeAccount(TEST_ACCOUNT_HOLDER_1);
        removeAccount(TEST_ACCOUNT_HOLDER_2);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Re-validate and run checks.
            mOAuth2TokenService.updateAccountList();
            Assert.assertEquals(2, mObserver.getAvailableCallCount());
            Assert.assertEquals(2, mObserver.getRevokedCallCount());
            Assert.assertEquals(0, mObserver.getLoadedCallCount());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsRegisteredAndOneSignedIn() {
        // Add accounts.
        addAccount(TEST_ACCOUNT_HOLDER_1);
        addAccount(TEST_ACCOUNT_HOLDER_2);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Mark user as signed in.
            mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

            // Run test.
            mOAuth2TokenService.updateAccountList();

            // All accounts will be notified. It is up to the observer
            // to design if any action is needed.
            Assert.assertEquals(2, mObserver.getAvailableCallCount());
            Assert.assertEquals(0, mObserver.getRevokedCallCount());
            Assert.assertEquals(0, mObserver.getLoadedCallCount());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListNoAccountsRegisteredButSignedIn() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Mark user as signed in without setting up the account.
            mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

            // Run test.
            mOAuth2TokenService.updateAccountList();

            // Ensure no calls have been made to the observer.
            Assert.assertEquals(0, mObserver.getAvailableCallCount());
            Assert.assertEquals(0, mObserver.getRevokedCallCount());
            Assert.assertEquals(0, mObserver.getLoadedCallCount());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListFiresEventAtTheEnd() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Mark user as signed in without setting up the account.
            mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);
            TestObserver ob = new TestObserver() {
                @Override
                public void onRefreshTokenAvailable(Account account) {
                    super.onRefreshTokenAvailable(account);
                    Assert.assertEquals(1, OAuth2TokenService.getAccounts().length);
                }
            };

            mOAuth2TokenService.addObserver(ob);
            mOAuth2TokenService.updateAccountList();
        });
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
