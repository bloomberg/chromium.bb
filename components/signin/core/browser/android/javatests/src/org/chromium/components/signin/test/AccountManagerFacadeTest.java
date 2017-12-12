// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test;

import android.accounts.Account;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.components.signin.AccountManagerDelegateException;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;

/**
 * Test class for {@link AccountManagerFacade}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class AccountManagerFacadeTest {
    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    private FakeAccountManagerDelegate mDelegate;
    private AccountManagerFacade mFacade;

    @Before
    public void setUp() throws Exception {
        mDelegate = new FakeAccountManagerDelegate(
                FakeAccountManagerDelegate.ENABLE_PROFILE_DATA_SOURCE);
        Assert.assertFalse(mDelegate.isRegisterObserversCalled());
        AccountManagerFacade.overrideAccountManagerFacadeForTests(mDelegate);
        Assert.assertTrue(mDelegate.isRegisterObserversCalled());
        mFacade = AccountManagerFacade.get();
    }

    @Test
    @SmallTest
    public void testCanonicalAccount() {
        addTestAccount("test@gmail.com");

        Assert.assertTrue(mFacade.hasAccountForName("test@gmail.com"));
        Assert.assertTrue(mFacade.hasAccountForName("Test@gmail.com"));
        Assert.assertTrue(mFacade.hasAccountForName("te.st@gmail.com"));
    }

    // If this test starts flaking, please re-open crbug.com/568636 and make sure there is some sort
    // of stack trace or error message in that bug BEFORE disabling the test.
    @Test
    @SmallTest
    public void testNonCanonicalAccount() {
        addTestAccount("test.me@gmail.com");

        Assert.assertTrue(mFacade.hasAccountForName("test.me@gmail.com"));
        Assert.assertTrue(mFacade.hasAccountForName("testme@gmail.com"));
        Assert.assertTrue(mFacade.hasAccountForName("Testme@gmail.com"));
        Assert.assertTrue(mFacade.hasAccountForName("te.st.me@gmail.com"));
    }

    @Test
    @MediumTest
    public void testGetAccountsMultipleAccounts() throws AccountManagerDelegateException {
        Assert.assertArrayEquals(new Account[] {}, mFacade.getGoogleAccounts());

        Account account = addTestAccount("test@gmail.com");
        Assert.assertArrayEquals(new Account[] {account}, mFacade.getGoogleAccounts());

        Account account2 = addTestAccount("test2@gmail.com");
        Assert.assertArrayEquals(new Account[] {account, account2}, mFacade.getGoogleAccounts());

        Account account3 = addTestAccount("test3@gmail.com");
        Assert.assertArrayEquals(
                new Account[] {account, account2, account3}, mFacade.getGoogleAccounts());

        removeTestAccount(account2);
        Assert.assertArrayEquals(new Account[] {account, account3}, mFacade.getGoogleAccounts());
    }

    @Test
    @MediumTest
    public void testGetAccountsChildAccountFiltering() throws AccountManagerDelegateException {
        Account account = addTestAccount("test@gmail.com");
        Assert.assertArrayEquals(new Account[] {account}, mFacade.getGoogleAccounts());

        Account childAccount = addChildTestAccount("child@gmail.com");
        Assert.assertArrayEquals(new Account[] {childAccount}, mFacade.getGoogleAccounts());

        Account account2 = addTestAccount("test2@gmail.com");
        Assert.assertArrayEquals(new Account[] {childAccount}, mFacade.getGoogleAccounts());

        // If child account is gone, non-child accounts should be exposed again
        removeTestAccount(childAccount);
        Assert.assertArrayEquals(new Account[] {account, account2}, mFacade.getGoogleAccounts());
    }

    @Test
    @SmallTest
    public void testProfileDataSource() throws Throwable {
        String accountName = "test@gmail.com";
        addTestAccount(accountName);

        mRule.runOnUiThread(() -> {
            ProfileDataSource.ProfileData profileData = new ProfileDataSource.ProfileData(
                    accountName, null, "Test Full Name", "Test Given Name");

            ProfileDataSource profileDataSource = mDelegate.getProfileDataSource();
            Assert.assertNotNull(profileDataSource);
            mDelegate.setProfileData(accountName, profileData);
            Assert.assertArrayEquals(profileDataSource.getProfileDataMap().values().toArray(),
                    new ProfileDataSource.ProfileData[] {profileData});

            mDelegate.setProfileData(accountName, null);
            Assert.assertArrayEquals(profileDataSource.getProfileDataMap().values().toArray(),
                    new ProfileDataSource.ProfileData[0]);
        });
    }

    private Account addTestAccount(String accountName) {
        Account account = AccountManagerFacade.createAccountFromName(accountName);
        AccountHolder holder = AccountHolder.builder(account).alwaysAccept(true).build();
        mDelegate.addAccountHolderBlocking(holder);
        return account;
    }

    private Account addChildTestAccount(String accountName) {
        Account account = AccountManagerFacade.createAccountFromName(accountName);
        AccountHolder holder =
                AccountHolder.builder(account)
                        .alwaysAccept(true)
                        .addFeature(AccountManagerFacade.FEATURE_IS_CHILD_ACCOUNT_KEY)
                        .build();
        mDelegate.addAccountHolderBlocking(holder);
        return account;
    }

    private void removeTestAccount(Account account) {
        mDelegate.removeAccountHolderBlocking(AccountHolder.builder(account).build());
    }
}
