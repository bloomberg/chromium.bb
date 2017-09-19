// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.accounts.Account;
import android.accounts.AuthenticatorDescription;
import android.app.Activity;
import android.content.Context;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;

import org.junit.Assert;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.AccountManagerDelegateException;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.ProfileDataSource;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.UUID;

/**
 * The FakeAccountManagerDelegate is intended for testing components that use AccountManagerFacade.
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
public class FakeAccountManagerDelegate implements AccountManagerDelegate {
    private static class FakeProfileDataSource implements ProfileDataSource {
        private final ObserverList<Observer> mObservers = new ObserverList<>();
        private final Map<String, ProfileData> mProfileDataMap = new HashMap<>();

        FakeProfileDataSource() {}

        @Override
        public Map<String, ProfileData> getProfileDataMap() {
            ThreadUtils.assertOnUiThread();
            return Collections.unmodifiableMap(mProfileDataMap);
        }

        @Override
        public @Nullable ProfileData getProfileDataForAccount(String accountId) {
            ThreadUtils.assertOnUiThread();
            return mProfileDataMap.get(accountId);
        }

        @Override
        public void addObserver(Observer observer) {
            ThreadUtils.assertOnUiThread();
            mObservers.addObserver(observer);
        }

        @Override
        public void removeObserver(Observer observer) {
            ThreadUtils.assertOnUiThread();
            boolean success = mObservers.removeObserver(observer);
            assert success : "Can't find observer";
        }

        public void setProfileData(String accountId, @Nullable ProfileData profileData) {
            ThreadUtils.assertOnUiThread();
            if (profileData == null) {
                mProfileDataMap.remove(accountId);
            } else {
                assert accountId.equals(profileData.getAccountName());
                mProfileDataMap.put(accountId, profileData);
            }
            fireOnProfileDataUpdatedNotification(accountId);
        }

        private void fireOnProfileDataUpdatedNotification(String accountId) {
            for (Observer observer : mObservers) {
                observer.onProfileDataUpdated(accountId);
            }
        }
    }

    private static final String TAG = "FakeAccountManager";

    /** Controls whether FakeAccountManagerDelegate should provide a ProfileDataSource. */
    @IntDef({DISABLE_PROFILE_DATA_SOURCE, ENABLE_PROFILE_DATA_SOURCE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ProfileDataSourceFlag {}

    /** Disables ProfileDataSource: {@link #getProfileDataSource} will return null. */
    public static final int DISABLE_PROFILE_DATA_SOURCE = 0;
    /** Use {@link FakeProfileDataSource}. */
    public static final int ENABLE_PROFILE_DATA_SOURCE = 1;

    private final Set<AccountHolder> mAccounts = new HashSet<>();
    private final ObserverList<AccountsChangeObserver> mObservers = new ObserverList<>();
    private boolean mRegisterObserversCalled;
    private FakeProfileDataSource mFakeProfileDataSource;

    @VisibleForTesting
    public FakeAccountManagerDelegate(@ProfileDataSourceFlag int profileDataSourceFlag) {
        if (profileDataSourceFlag == ENABLE_PROFILE_DATA_SOURCE) {
            mFakeProfileDataSource = new FakeProfileDataSource();
        }
    }

    /** Will be removed after fixing downstream clients. */
    @Deprecated
    public FakeAccountManagerDelegate(Context context, Account... accounts) {
        if (accounts != null) {
            for (Account account : accounts) {
                mAccounts.add(AccountHolder.builder(account).alwaysAccept(true).build());
            }
        }
    }

    public void setProfileData(
            String accountId, @Nullable ProfileDataSource.ProfileData profileData) {
        assert mFakeProfileDataSource != null : "ProfileDataSource was disabled!";
        mFakeProfileDataSource.setProfileData(accountId, profileData);
    }

    @Nullable
    @Override
    public ProfileDataSource getProfileDataSource() {
        return mFakeProfileDataSource;
    }

    @Override
    public void registerObservers() {
        mRegisterObserversCalled = true;
    }

    public boolean isRegisterObserversCalled() {
        return mRegisterObserversCalled;
    }

    @Override
    public void addObserver(AccountsChangeObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(AccountsChangeObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public Account[] getAccountsSync() throws AccountManagerDelegateException {
        return getAccountsSyncNoThrow();
    }

    public Account[] getAccountsSyncNoThrow() {
        ArrayList<Account> result = new ArrayList<>();
        for (AccountHolder ah : mAccounts) {
            result.add(ah.getAccount());
        }
        return result.toArray(new Account[0]);
    }

    /**
     * Add an AccountHolder directly.
     *
     * @param accountHolder the account holder to add
     */
    public void addAccountHolderExplicitly(AccountHolder accountHolder) {
        boolean added = mAccounts.add(accountHolder);
        Assert.assertTrue("Account was already added", added);
        for (AccountsChangeObserver observer : mObservers) {
            observer.onAccountsChanged();
        }
    }

    /**
     * Remove an AccountHolder directly.
     *
     * @param accountHolder the account holder to remove
     */
    public void removeAccountHolderExplicitly(AccountHolder accountHolder) {
        boolean removed = mAccounts.remove(accountHolder);
        Assert.assertTrue("Account was already added", removed);
        for (AccountsChangeObserver observer : mObservers) {
            observer.onAccountsChanged();
        }
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
                Log.d(TAG,
                        "Created new auth token for " + ah.getAccount() + ": authTokenScope = "
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
                AccountManagerFacade.GOOGLE_ACCOUNT_TYPE, "p1", 0, 0, 0, 0);

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
}
