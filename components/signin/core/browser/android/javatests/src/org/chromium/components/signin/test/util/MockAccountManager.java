// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.accounts.Account;
import android.accounts.AuthenticatorDescription;
import android.app.Activity;
import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.AccountManagerHelper;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;
import java.util.UUID;

/**
 * The MockAccountManager helps out if you want to mock out all calls to the Android AccountManager.
 *
 * You should provide a set of accounts as a constructor argument, or use the more direct approach
 * and provide an array of AccountHolder objects.
 *
 * Currently, this implementation supports adding and removing accounts, handling credentials
 * (including confirming them), and handling of dummy auth tokens.
 *
 * If you want to auto-approve a given authtokentype, use addAccountHolderExplicitly(...) with
 * an AccountHolder you have built with hasBeenAccepted("yourAuthTokenType", true).
 *
 * If you want to auto-approve all auth token types for a given account, use the {@link
 * AccountHolder} builder method alwaysAccept(true).
 */
public class MockAccountManager implements AccountManagerDelegate {
    private static final String TAG = "MockAccountManager";

    protected final Context mContext;

    private final Set<AccountHolder> mAccounts = new HashSet<>();

    // Tracks the number of in-progress getAccountsByType() tasks so that tests can wait for
    // their completion.
    private final ZeroCounter mGetAccountsTaskCounter;

    @VisibleForTesting
    public MockAccountManager(Context context, Context testContext, Account... accounts) {
        mContext = context;
        mGetAccountsTaskCounter = new ZeroCounter();
        if (accounts != null) {
            for (Account account : accounts) {
                mAccounts.add(AccountHolder.builder(account).alwaysAccept(true).build());
            }
        }
    }

    @Override
    public Account[] getAccountsByType(String type) {
        if (!AccountManagerHelper.GOOGLE_ACCOUNT_TYPE.equals(type)) {
            throw new IllegalArgumentException("Invalid account type: " + type);
        }
        ArrayList<Account> validAccounts = new ArrayList<>();
        for (AccountHolder ah : mAccounts) {
            if (type.equals(ah.getAccount().type)) {
                validAccounts.add(ah.getAccount());
            }
        }
        return validAccounts.toArray(new Account[0]);
    }

    @VisibleForTesting
    public void waitForGetAccountsTask() throws InterruptedException {
        // Wait until all tasks are done because we don't know which is being waited for.
        mGetAccountsTaskCounter.waitUntilZero();
    }

    /**
     * Add an AccountHolder directly.
     *
     * @param accountHolder the account holder to add
     * @return whether the account holder was added successfully
     */
    public boolean addAccountHolderExplicitly(AccountHolder accountHolder) {
        return mAccounts.add(accountHolder);
    }

    /**
     * Remove an AccountHolder directly.
     *
     * @param accountHolder the account holder to remove
     * @return whether the account holder was removed successfully
     */
    public boolean removeAccountHolderExplicitly(AccountHolder accountHolder) {
        return mAccounts.remove(accountHolder);
    }

    @Override
    public String getAuthToken(Account account, String authTokenScope) {
        AccountHolder ah = getAccountHolder(account);
        assert ah.hasBeenAccepted(authTokenScope);
        synchronized (mAccounts) {
            // Some tests register auth tokens with value null, and those should be preserved.
            if (!ah.hasAuthTokenRegistered(authTokenScope)
                    && ah.getAuthToken(authTokenScope) == null) {
                // No authtoken registered. Need to create one.
                String authToken = UUID.randomUUID().toString();
                Log.d(TAG, "Created new auth token for " + ah.getAccount() + ": authTokenScope = "
                                + authTokenScope + ", authToken = " + authToken);
                ah = ah.withAuthToken(authTokenScope, authToken);
                mAccounts.add(ah);
            }
        }
        return ah.getAuthToken(authTokenScope);
    }

    @Override
    public void invalidateAuthToken(String authToken) {
        if (authToken == null) {
            throw new IllegalArgumentException("AuthToken can not be null");
        }
        for (AccountHolder ah : mAccounts) {
            if (ah.removeAuthToken(authToken)) {
                break;
            }
        }
    }

    @Override
    public AuthenticatorDescription[] getAuthenticatorTypes() {
        AuthenticatorDescription googleAuthenticator = new AuthenticatorDescription(
                AccountManagerHelper.GOOGLE_ACCOUNT_TYPE, "p1", 0, 0, 0, 0);

        return new AuthenticatorDescription[] {googleAuthenticator};
    }

    @Override
    public boolean hasFeatures(Account account, String[] features) {
        final AccountHolder accountHolder = getAccountHolder(account);
        Set<String> accountFeatures = accountHolder.getFeatures();
        boolean hasAllFeatures = true;
        for (String feature : features) {
            if (!accountFeatures.contains(feature)) {
                Log.d(TAG, accountFeatures + " does not contain " + feature);
                hasAllFeatures = false;
            }
        }
        return hasAllFeatures;
    }

    @Override
    public void updateCredentials(
            Account account, Activity activity, final Callback<Boolean> callback) {
        ThreadUtils.assertOnUiThread();
        if (callback == null) {
            return;
        }

        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                callback.onResult(true);
            }
        });
    }

    private AccountHolder getAccountHolder(Account account) {
        if (account == null) {
            throw new IllegalArgumentException("Account can not be null");
        }
        for (AccountHolder accountHolder : mAccounts) {
            if (account.equals(accountHolder.getAccount())) {
                return accountHolder;
            }
        }
        throw new IllegalArgumentException("Can not find AccountHolder for account " + account);
    }

    /**
     * Simple concurrency helper class for waiting until a resource count becomes zero.
     */
    private static class ZeroCounter {
        private static final int WAIT_TIMEOUT_MS = 10000;

        private final Object mLock = new Object();
        private int mCount = 0;

        public void increment() {
            synchronized (mLock) {
                mCount++;
            }
        }

        public void decrement() {
            synchronized (mLock) {
                if (--mCount == 0) {
                    mLock.notifyAll();
                }
            }
        }

        public void waitUntilZero() throws InterruptedException {
            synchronized (mLock) {
                while (mCount != 0) {
                    mLock.wait(WAIT_TIMEOUT_MS);
                }
            }
        }
    }
}
