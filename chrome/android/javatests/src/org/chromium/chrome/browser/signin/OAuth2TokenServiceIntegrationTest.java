// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.ApplicationData;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.content.browser.test.NativeLibraryTestRule;

import java.util.concurrent.Callable;

/**
 * Integration test for the OAuth2TokenService.
 *
 * These tests initialize the native part of the service.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class OAuth2TokenServiceIntegrationTest {
    @Rule
    public NativeLibraryTestRule mActivityTestRule = new NativeLibraryTestRule();

    @Rule
    public UiThreadTestRule mUiThreadTestRule = new UiThreadTestRule();

    private static final Account TEST_ACCOUNT1 =
            AccountManagerFacade.createAccountFromName("foo@gmail.com");
    private static final Account TEST_ACCOUNT2 =
            AccountManagerFacade.createAccountFromName("bar@gmail.com");
    private static final AccountHolder TEST_ACCOUNT_HOLDER_1 =
            AccountHolder.builder(TEST_ACCOUNT1).alwaysAccept(true).build();
    private static final AccountHolder TEST_ACCOUNT_HOLDER_2 =
            AccountHolder.builder(TEST_ACCOUNT2).alwaysAccept(true).build();

    private AdvancedMockContext mContext;
    private OAuth2TokenService mOAuth2TokenService;
    private FakeAccountManagerDelegate mAccountManager;
    private TestObserver mObserver;
    private ChromeSigninController mChromeSigninController;

    @Before
    public void setUp() throws Exception {
        mapAccountNamesToIds();
        ApplicationData.clearAppData(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        // loadNativeLibraryAndInitBrowserProcess will access AccountManagerFacade, so it should
        // be initialized beforehand.
        mContext = new AdvancedMockContext(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        mAccountManager = new FakeAccountManagerDelegate(
                FakeAccountManagerDelegate.DISABLE_PROFILE_DATA_SOURCE);
        AccountManagerFacade.overrideAccountManagerFacadeForTests(mAccountManager);

        mActivityTestRule.loadNativeLibraryAndInitBrowserProcess();

        // Make sure there is no account signed in yet.
        mChromeSigninController = ChromeSigninController.get();
        mChromeSigninController.setSignedInAccountName(null);

        // Seed test accounts to AccountTrackerService.
        seedAccountTrackerService(mContext);

        // Get a reference to the service.
        mOAuth2TokenService = getOAuth2TokenServiceOnUiThread();

        // Set up observer.
        mObserver = new TestObserver();
        addObserver(mObserver);
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mChromeSigninController.setSignedInAccountName(null);
                mOAuth2TokenService.validateAccounts(false);
            }
        });
        AccountManagerFacade.resetAccountManagerFacadeForTests();
    }

    private void mapAccountNamesToIds() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
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
            }
        });
    }

    private void seedAccountTrackerService(final Context context) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                AccountIdProvider provider = AccountIdProvider.getInstance();
                String[] accountNames = {TEST_ACCOUNT1.name, TEST_ACCOUNT2.name};
                String[] accountIds = {provider.getAccountId(accountNames[0]),
                        provider.getAccountId(accountNames[1])};
                AccountTrackerService.get().syncForceRefreshForTest(accountIds, accountNames);
            }
        });
    }

    /**
     * The {@link OAuth2TokenService} and the {@link Profile} can only be accessed from the UI
     * thread, so this helper method is a convenience method to retrieve it.
     *
     * @return the OAuth2TokenService.
     */
    private static OAuth2TokenService getOAuth2TokenServiceOnUiThread() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<OAuth2TokenService>() {
            @Override
            public OAuth2TokenService call() throws Exception {
                return OAuth2TokenService.getForProfile(Profile.getLastUsedProfile());
            }
        });
    }

    private void addObserver(final TestObserver observer) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mOAuth2TokenService.addObserver(observer);
            }
        });
    }

    @Test
    @MediumTest
    @Feature({"Sync"})
    public void testFireRefreshTokenAvailableNotifiesJavaObservers() throws Throwable {
        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Adding an observer should not lead to a callback.
                Assert.assertEquals(0, mObserver.getAvailableCallCount());

                // An observer should be called with the correct account.
                mOAuth2TokenService.fireRefreshTokenAvailable(TEST_ACCOUNT1);
                Assert.assertEquals(1, mObserver.getAvailableCallCount());
                Assert.assertEquals(TEST_ACCOUNT1, mObserver.getLastAccount());

                // When mOAuth2TokenService, an observer should not be called.
                mOAuth2TokenService.removeObserver(mObserver);
                mOAuth2TokenService.fireRefreshTokenAvailable(TEST_ACCOUNT1);
                Assert.assertEquals(1, mObserver.getAvailableCallCount());

                // No other observer interface method should ever have been called.
                Assert.assertEquals(0, mObserver.getRevokedCallCount());
                Assert.assertEquals(0, mObserver.getLoadedCallCount());
            }
        });
    }

    @Test
    @MediumTest
    @Feature({"Sync"})
    public void testFireRefreshTokenRevokedNotifiesJavaObservers() throws Throwable {
        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Adding an observer should not lead to a callback.
                Assert.assertEquals(0, mObserver.getRevokedCallCount());

                // An observer should be called with the correct account.
                mOAuth2TokenService.fireRefreshTokenRevoked(TEST_ACCOUNT1);
                Assert.assertEquals(1, mObserver.getRevokedCallCount());
                Assert.assertEquals(TEST_ACCOUNT1, mObserver.getLastAccount());

                // When removed, an observer should not be called.
                mOAuth2TokenService.removeObserver(mObserver);
                mOAuth2TokenService.fireRefreshTokenRevoked(TEST_ACCOUNT2);
                Assert.assertEquals(1, mObserver.getRevokedCallCount());

                // No other observer interface method should ever have been called.
                Assert.assertEquals(0, mObserver.getAvailableCallCount());
                Assert.assertEquals(0, mObserver.getLoadedCallCount());
            }
        });
    }

    @Test
    @MediumTest
    @Feature({"Sync"})
    public void testFireRefreshTokensLoadedNotifiesJavaObservers() throws Throwable {
        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Adding an observer should not lead to a callback.
                Assert.assertEquals(0, mObserver.getLoadedCallCount());

                // An observer should be called with the correct account.
                mOAuth2TokenService.fireRefreshTokensLoaded();
                Assert.assertEquals(1, mObserver.getLoadedCallCount());

                // When removed, an observer should not be called.
                mOAuth2TokenService.removeObserver(mObserver);
                mOAuth2TokenService.fireRefreshTokensLoaded();
                Assert.assertEquals(1, mObserver.getLoadedCallCount());

                // No other observer interface method should ever have been called.
                Assert.assertEquals(0, mObserver.getAvailableCallCount());
                Assert.assertEquals(0, mObserver.getRevokedCallCount());
            }
        });
    }

    @Test
    @MediumTest
    public void testValidateAccountsNoAccountsRegisteredAndNoSignedInUser() throws Throwable {
        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Run test.
                mOAuth2TokenService.validateAccounts(false);

                // Ensure no calls have been made to the observer.
                Assert.assertEquals(0, mObserver.getAvailableCallCount());
                Assert.assertEquals(0, mObserver.getRevokedCallCount());
                Assert.assertEquals(0, mObserver.getLoadedCallCount());
            }
        });
    }

    @Test
    @MediumTest
    public void testValidateAccountsOneAccountsRegisteredAndNoSignedInUser() throws Throwable {
        mAccountManager.addAccountHolderBlocking(TEST_ACCOUNT_HOLDER_1);

        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Run test.
                mOAuth2TokenService.validateAccounts(false);

                // Ensure no calls have been made to the observer.
                Assert.assertEquals(0, mObserver.getAvailableCallCount());
                Assert.assertEquals(0, mObserver.getRevokedCallCount());
                Assert.assertEquals(0, mObserver.getLoadedCallCount());
            }
        });
    }

    @Test
    @MediumTest
    public void testValidateAccountsOneAccountsRegisteredSignedIn() throws Throwable {
        mAccountManager.addAccountHolderBlocking(TEST_ACCOUNT_HOLDER_1);

        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Mark user as signed in.
                mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

                // Run test.
                mOAuth2TokenService.validateAccounts(false);

                // Ensure one call for the signed in account.
                Assert.assertEquals(1, mObserver.getAvailableCallCount());
                Assert.assertEquals(0, mObserver.getRevokedCallCount());
                Assert.assertEquals(0, mObserver.getLoadedCallCount());

                // Validate again and make sure no new calls are made.
                mOAuth2TokenService.validateAccounts(false);
                Assert.assertEquals(1, mObserver.getAvailableCallCount());
                Assert.assertEquals(0, mObserver.getRevokedCallCount());
                Assert.assertEquals(0, mObserver.getLoadedCallCount());

                // Validate again with force notifications and make sure one new calls is made.
                mOAuth2TokenService.validateAccounts(true);
                Assert.assertEquals(2, mObserver.getAvailableCallCount());
                Assert.assertEquals(0, mObserver.getRevokedCallCount());
                Assert.assertEquals(0, mObserver.getLoadedCallCount());
            }
        });
    }

    @Test
    @MediumTest
    public void testValidateAccountsSingleAccountWithoutChanges() throws Throwable {
        mAccountManager.addAccountHolderBlocking(TEST_ACCOUNT_HOLDER_1);

        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Mark user as signed in.
                mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

                // Run one validation.
                mOAuth2TokenService.validateAccounts(false);
                Assert.assertEquals(1, mObserver.getAvailableCallCount());
                Assert.assertEquals(0, mObserver.getRevokedCallCount());
                Assert.assertEquals(0, mObserver.getLoadedCallCount());

                // Re-run validation.
                mOAuth2TokenService.validateAccounts(false);
                Assert.assertEquals(1, mObserver.getAvailableCallCount());
                Assert.assertEquals(0, mObserver.getRevokedCallCount());
                Assert.assertEquals(0, mObserver.getLoadedCallCount());
            }
        });
    }

    @Test
    @MediumTest
    public void testValidateAccountsSingleAccountThenAddOne() throws Throwable {
        mAccountManager.addAccountHolderBlocking(TEST_ACCOUNT_HOLDER_1);

        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Mark user as signed in.
                mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

                // Run one validation.
                mOAuth2TokenService.validateAccounts(false);
                Assert.assertEquals(1, mObserver.getAvailableCallCount());
                Assert.assertEquals(0, mObserver.getRevokedCallCount());
                Assert.assertEquals(0, mObserver.getLoadedCallCount());
            }
        });

        // Add another account.
        mAccountManager.addAccountHolderBlocking(TEST_ACCOUNT_HOLDER_2);

        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Seed AccountTrackerService again since accounts changed after last validation.
                seedAccountTrackerService(mContext);

                // Re-run validation.
                mOAuth2TokenService.validateAccounts(false);
                Assert.assertEquals(2, mObserver.getAvailableCallCount());
                Assert.assertEquals(0, mObserver.getRevokedCallCount());
                Assert.assertEquals(0, mObserver.getLoadedCallCount());
            }
        });
    }

    @Test
    @MediumTest
    public void testValidateAccountsTwoAccountsThenRemoveOne() throws Throwable {
        // Add accounts.
        mAccountManager.addAccountHolderBlocking(TEST_ACCOUNT_HOLDER_1);
        mAccountManager.addAccountHolderBlocking(TEST_ACCOUNT_HOLDER_2);

        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Mark user as signed in.
                mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

                // Run one validation.
                mOAuth2TokenService.validateAccounts(false);
                Assert.assertEquals(2, mObserver.getAvailableCallCount());
            }
        });

        mAccountManager.removeAccountHolderBlocking(TEST_ACCOUNT_HOLDER_2);

        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mOAuth2TokenService.validateAccounts(false);

                Assert.assertEquals(2, mObserver.getAvailableCallCount());
                Assert.assertEquals(1, mObserver.getRevokedCallCount());
                Assert.assertEquals(0, mObserver.getLoadedCallCount());
            }
        });
    }

    @Test
    @MediumTest
    public void testValidateAccountsTwoAccountsThenRemoveAll() throws Throwable {
        // Add accounts.
        mAccountManager.addAccountHolderBlocking(TEST_ACCOUNT_HOLDER_1);
        mAccountManager.addAccountHolderBlocking(TEST_ACCOUNT_HOLDER_2);

        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Mark user as signed in.
                mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

                mOAuth2TokenService.validateAccounts(false);
                Assert.assertEquals(2, mObserver.getAvailableCallCount());
            }
        });

        // Remove all.
        mAccountManager.removeAccountHolderBlocking(TEST_ACCOUNT_HOLDER_1);
        mAccountManager.removeAccountHolderBlocking(TEST_ACCOUNT_HOLDER_2);

        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Re-validate and run checks.
                mOAuth2TokenService.validateAccounts(false);
                Assert.assertEquals(2, mObserver.getRevokedCallCount());
                Assert.assertEquals(0, mObserver.getLoadedCallCount());
            }
        });
    }

    @Test
    @MediumTest
    @RetryOnFailure
    public void testValidateAccountsTwoAccountsThenRemoveAllSignOut() throws Throwable {
        // Add accounts.
        mAccountManager.addAccountHolderBlocking(TEST_ACCOUNT_HOLDER_1);
        mAccountManager.addAccountHolderBlocking(TEST_ACCOUNT_HOLDER_2);

        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Mark user as signed in.
                mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

                mOAuth2TokenService.validateAccounts(false);
                Assert.assertEquals(2, mObserver.getAvailableCallCount());

                // Remove all.
                mChromeSigninController.setSignedInAccountName(null);
            }
        });

        mAccountManager.removeAccountHolderBlocking(TEST_ACCOUNT_HOLDER_1);
        mAccountManager.removeAccountHolderBlocking(TEST_ACCOUNT_HOLDER_2);

        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Re-validate and run checks.
                mOAuth2TokenService.validateAccounts(false);
                Assert.assertEquals(2, mObserver.getRevokedCallCount());
                Assert.assertEquals(0, mObserver.getLoadedCallCount());
            }
        });
    }

    @Test
    @MediumTest
    public void testValidateAccountsTwoAccountsRegisteredAndOneSignedIn() throws Throwable {
        // Add accounts.
        mAccountManager.addAccountHolderBlocking(TEST_ACCOUNT_HOLDER_1);
        mAccountManager.addAccountHolderBlocking(TEST_ACCOUNT_HOLDER_2);

        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Mark user as signed in.
                mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

                // Run test.
                mOAuth2TokenService.validateAccounts(false);

                // All accounts will be notified. It is up to the observer
                // to design if any action is needed.
                Assert.assertEquals(2, mObserver.getAvailableCallCount());
                Assert.assertEquals(0, mObserver.getRevokedCallCount());
                Assert.assertEquals(0, mObserver.getLoadedCallCount());
            }
        });
    }

    @Test
    @MediumTest
    public void testValidateAccountsNoAccountsRegisteredButSignedIn() throws Throwable {
        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Mark user as signed in without setting up the account.
                mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

                // Run test.
                mOAuth2TokenService.validateAccounts(false);

                // Ensure no calls have been made to the observer.
                Assert.assertEquals(0, mObserver.getAvailableCallCount());
                Assert.assertEquals(0, mObserver.getRevokedCallCount());
                Assert.assertEquals(0, mObserver.getLoadedCallCount());
            }
        });
    }

    @Test
    @MediumTest
    public void testValidateAccountsFiresEventAtTheEnd() throws Throwable {
        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Mark user as signed in without setting up the account.
                mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);
                TestObserver ob = new TestObserver() {
                    @Override
                    public void onRefreshTokenAvailable(Account account) {
                        super.onRefreshTokenAvailable(account);
                        Assert.assertEquals(1, OAuth2TokenService.getAccounts().length);
                    }
                };

                addObserver(ob);
                mOAuth2TokenService.validateAccounts(false);
            }
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
