// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.accounts.Account;
import android.os.Handler;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.annotation.Nullable;

/**
 * This class is used by the {@link MockAccountManager} to hold information about a given
 * account, such as its password and set of granted auth tokens.
 */
public class AccountHolder {
    private final Account mAccount;

    private final String mPassword;

    private final Map<String, String> mAuthTokens;

    private final Map<String, Boolean> mHasBeenAccepted;

    private final boolean mAlwaysAccept;

    private Set<String> mFeatures;

    private final List<Runnable> mFeatureCallbacks = new ArrayList<>();

    private AccountHolder(Account account, String password, Map<String, String> authTokens,
            Map<String, Boolean> hasBeenAccepted, boolean alwaysAccept,
            @Nullable Set<String> features) {
        if (account == null) {
            throw new IllegalArgumentException("Account can not be null");
        }
        mAccount = account;
        mPassword = password;
        mAuthTokens = authTokens == null ? new HashMap<String, String>() : authTokens;
        mHasBeenAccepted =
                hasBeenAccepted == null ? new HashMap<String, Boolean>() : hasBeenAccepted;
        mAlwaysAccept = alwaysAccept;
        mFeatures = features;
    }

    public Account getAccount() {
        return mAccount;
    }

    public String getPassword() {
        return mPassword;
    }

    public boolean hasAuthTokenRegistered(String authTokenType) {
        return mAuthTokens.containsKey(authTokenType);
    }

    public String getAuthToken(String authTokenType) {
        return mAuthTokens.get(authTokenType);
    }

    public boolean hasBeenAccepted(String authTokenType) {
        return mAlwaysAccept
                || mHasBeenAccepted.containsKey(authTokenType)
                && mHasBeenAccepted.get(authTokenType);
    }

    /**
     * Removes an auth token from the auth token map.
     *
     * @param authToken the auth token to remove
     * @return true if the auth token was found
     */
    public boolean removeAuthToken(String authToken) {
        String foundKey = null;
        for (Map.Entry<String, String> tokenEntry : mAuthTokens.entrySet()) {
            if (authToken.equals(tokenEntry.getValue())) {
                foundKey = tokenEntry.getKey();
                break;
            }
        }
        if (foundKey == null) {
            return false;
        } else {
            mAuthTokens.remove(foundKey);
            return true;
        }
    }

    /**
     * @return The set of account features. This method may only be called after the account
     *         features have been fetched.
     */
    public Set<String> getFeatures() {
        assert mFeatures != null;
        return mFeatures;
    }

    /**
     * Adds a callback to be run when the account features have been fetched. If that has already
     * happened, the callback is run immediately.
     *
     * @param callback The callback to be run when the account features have been fetched.
     */
    public void addFeaturesCallback(Runnable callback) {
        if (mFeatures == null) {
            mFeatureCallbacks.add(callback);
            return;
        }

        new Handler().post(callback);
    }

    /**
     * Notifies this object that the account features have been fetched.
     *
     * @param features The set of account features.
     */
    public void didFetchFeatures(Set<String> features) {
        assert features != null;
        assert mFeatures == null;
        mFeatures = features;
        Handler handler = new Handler();
        for (Runnable r : mFeatureCallbacks) {
            handler.post(r);
        }
        mFeatureCallbacks.clear();
    }

    @Override
    public int hashCode() {
        return mAccount.hashCode();
    }

    @Override
    public boolean equals(Object that) {
        return that instanceof AccountHolder
                && mAccount.equals(((AccountHolder) that).getAccount());
    }

    public static Builder create() {
        return new Builder();
    }

    public AccountHolder withPassword(String password) {
        return copy().password(password).build();
    }

    public AccountHolder withAuthTokens(Map<String, String> authTokens) {
        return copy().authTokens(authTokens).build();
    }

    public AccountHolder withAuthToken(String authTokenType, String authToken) {
        return copy().authToken(authTokenType, authToken).build();
    }

    public AccountHolder withHasBeenAccepted(String authTokenType, boolean hasBeenAccepted) {
        return copy().hasBeenAccepted(authTokenType, hasBeenAccepted).build();
    }

    public AccountHolder withAlwaysAccept(boolean alwaysAccept) {
        return copy().alwaysAccept(alwaysAccept).build();
    }

    private Builder copy() {
        return create()
                .account(mAccount)
                .password(mPassword)
                .authTokens(mAuthTokens)
                .hasBeenAcceptedMap(mHasBeenAccepted)
                .alwaysAccept(mAlwaysAccept);
    }

    /**
     * Used to construct AccountHolder instances.
     */
    public static class Builder {
        private Account mTempAccount;

        private String mTempPassword;

        private Map<String, String> mTempAuthTokens;

        private Map<String, Boolean> mTempHasBeenAccepted;

        private boolean mTempAlwaysAccept;

        private Set<String> mFeatures = new HashSet<>();

        public Builder account(Account account) {
            mTempAccount = account;
            return this;
        }

        public Builder password(String password) {
            mTempPassword = password;
            return this;
        }

        public Builder authToken(String authTokenType, String authToken) {
            if (mTempAuthTokens == null) {
                mTempAuthTokens = new HashMap<String, String>();
            }
            mTempAuthTokens.put(authTokenType, authToken);
            return this;
        }

        public Builder authTokens(Map<String, String> authTokens) {
            mTempAuthTokens = authTokens;
            return this;
        }

        public Builder hasBeenAccepted(String authTokenType, boolean hasBeenAccepted) {
            if (mTempHasBeenAccepted == null) {
                mTempHasBeenAccepted = new HashMap<String, Boolean>();
            }
            mTempHasBeenAccepted.put(authTokenType, hasBeenAccepted);
            return this;
        }

        public Builder hasBeenAcceptedMap(Map<String, Boolean> hasBeenAcceptedMap) {
            mTempHasBeenAccepted = hasBeenAcceptedMap;
            return this;
        }

        public Builder alwaysAccept(boolean alwaysAccept) {
            mTempAlwaysAccept = alwaysAccept;
            return this;
        }

        public Builder addFeature(String feature) {
            if (mFeatures == null) {
                mFeatures = new HashSet<>();
            }
            mFeatures.add(feature);
            return this;
        }

        /**
         * Sets the set of features for this account.
         *
         * @param features The set of account features. Can be null to indicate that the account
         *            features have not been fetched yet. In this case,
         *            {@link AccountHolder#didFetchFeatures} should be called on the resulting
         *            {@link AccountHolder} before the features can be accessed.
         * @return This object, for chaining method calls.
         */
        public Builder featureSet(Set<String> features) {
            mFeatures = features;
            return this;
        }

        public AccountHolder build() {
            return new AccountHolder(mTempAccount, mTempPassword, mTempAuthTokens,
                    mTempHasBeenAccepted, mTempAlwaysAccept, mFeatures);
        }
    }
}
