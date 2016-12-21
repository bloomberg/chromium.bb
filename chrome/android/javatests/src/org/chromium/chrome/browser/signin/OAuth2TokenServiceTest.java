// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.components.signin.AccountManagerHelper;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.MockAccountManager;

import java.util.Arrays;
import java.util.concurrent.TimeUnit;

/** Tests for OAuth2TokenService. */
public class OAuth2TokenServiceTest extends InstrumentationTestCase {

    private AdvancedMockContext mContext;
    private MockAccountManager mAccountManager;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        // Mock out the account manager on the device.
        mContext = new AdvancedMockContext(getInstrumentation().getTargetContext());
        mAccountManager = new MockAccountManager(mContext, getInstrumentation().getContext());
        AccountManagerHelper.overrideAccountManagerHelperForTests(mContext, mAccountManager);
    }

    /*
     *  @SmallTest
     *  @Feature({"Sync"})
     */
    @DisabledTest(message = "crbug.com/533417")
    public void testGetAccountsNoAccountsRegistered() {
        String[] accounts = OAuth2TokenService.getAccounts(mContext);
        assertEquals("There should be no accounts registered", 0, accounts.length);
    }

    /*@SmallTest
    @Feature({"Sync"})*/
    @DisabledTest(message = "crbug.com/527852")
    public void testGetAccountsOneAccountRegistered() {
        Account account1 = AccountManagerHelper.createAccountFromName("foo@gmail.com");
        AccountHolder accountHolder1 = AccountHolder.create().account(account1).build();
        mAccountManager.addAccountHolderExplicitly(accountHolder1);

        String[] sysAccounts = OAuth2TokenService.getSystemAccountNames(mContext);
        assertEquals("There should be one registered account", 1, sysAccounts.length);
        assertEquals("The account should be " + account1, account1.name, sysAccounts[0]);

        String[] accounts = OAuth2TokenService.getAccounts(mContext);
        assertEquals("There should be zero registered account", 0, accounts.length);
    }

    /*@SmallTest
    @Feature({"Sync"})*/
    @DisabledTest(message = "crbug.com/527852")
    public void testGetAccountsTwoAccountsRegistered() {
        Account account1 = AccountManagerHelper.createAccountFromName("foo@gmail.com");
        AccountHolder accountHolder1 = AccountHolder.create().account(account1).build();
        mAccountManager.addAccountHolderExplicitly(accountHolder1);
        Account account2 = AccountManagerHelper.createAccountFromName("bar@gmail.com");
        AccountHolder accountHolder2 = AccountHolder.create().account(account2).build();
        mAccountManager.addAccountHolderExplicitly(accountHolder2);

        String[] sysAccounts = OAuth2TokenService.getSystemAccountNames(mContext);
        assertEquals("There should be one registered account", 2, sysAccounts.length);
        assertTrue("The list should contain " + account1,
                Arrays.asList(sysAccounts).contains(account1.name));
        assertTrue("The list should contain " + account2,
                Arrays.asList(sysAccounts).contains(account2.name));

        String[] accounts = OAuth2TokenService.getAccounts(mContext);
        assertEquals("There should be zero registered account", 0, accounts.length);
    }

    @DisabledTest(message = "crbug.com/568620")
    @SmallTest
    @Feature({"Sync"})
    public void testGetOAuth2AccessTokenWithTimeoutOnSuccess() {
        String authToken = "someToken";
        // Auth token should be successfully received.
        runTestOfGetOAuth2AccessTokenWithTimeout(authToken);
    }

    /*@SmallTest
    @Feature({"Sync"})*/
    @DisabledTest(message = "crbug.com/527852")
    public void testGetOAuth2AccessTokenWithTimeoutOnError() {
        String authToken = null;
        // Should not crash when auth token is null.
        runTestOfGetOAuth2AccessTokenWithTimeout(authToken);
    }

    private void runTestOfGetOAuth2AccessTokenWithTimeout(String expectedToken) {
        String scope = "http://example.com/scope";
        Account account = AccountManagerHelper.createAccountFromName("test@gmail.com");
        String oauth2Scope = "oauth2:" + scope;

        // Add an account with given auth token for the given scope, already accepted auth popup.
        AccountHolder accountHolder =
                AccountHolder.create()
                        .account(account)
                        .hasBeenAccepted(oauth2Scope, true)
                        .authToken(oauth2Scope, expectedToken).build();
        mAccountManager.addAccountHolderExplicitly(accountHolder);

        String accessToken = OAuth2TokenService.getOAuth2AccessTokenWithTimeout(
                mContext, account, scope, 5, TimeUnit.SECONDS);
        assertEquals(expectedToken, accessToken);
    }
}
