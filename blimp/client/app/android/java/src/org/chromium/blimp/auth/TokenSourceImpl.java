// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.auth;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.preference.PreferenceManager;

import com.google.android.gms.auth.GoogleAuthException;
import com.google.android.gms.auth.GoogleAuthUtil;
import com.google.android.gms.auth.GooglePlayServicesAvailabilityException;
import com.google.android.gms.auth.UserRecoverableNotifiedException;
import com.google.android.gms.common.ConnectionResult;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.blimp.R;

import java.io.IOException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * An implementation of TokenSource that handles querying {@link GoogleAuthUtil} for a valid
 * authentication token for a particular user account.  The user account will be saved as an
 * application preference once set.  See {@link TokenSource} for information on how callers know
 * whether or not an account is set and how they set one.
 */
public class TokenSourceImpl implements TokenSource {
    private static final String ACCOUNT_NAME_PREF = "BlimpAccount";
    // Prefix with oauth2: to make sure we get back an oauth2 token.
    private static final String ACCOUNT_OAUTH2_SCOPE =
            "oauth2:https://www.googleapis.com/auth/userinfo.email";
    private static final String ACCOUNT_TYPE = "com.google";
    private static final String TAG = "BlimpTokenSource";

    private final Context mAppContext;

    private TokenSource.Callback mCallback;
    private AsyncTask<String, Void, String> mTokenQueryTask;

    /**
     * Creates a {@link TokenSourceImpl} that will load and save account information from the
     * application {@link Context} given by {@code context}.
     * @param context
     */
    public TokenSourceImpl(Context context) {
        mAppContext = context.getApplicationContext();
    }

    // TokenSource implementation.
    @Override
    public void destroy() {
        ThreadUtils.assertOnUiThread();

        if (isRetrievingToken()) {
            mTokenQueryTask.cancel(true);
        }
    }

    @Override
    public void setCallback(TokenSource.Callback callback) {
        ThreadUtils.assertOnUiThread();

        mCallback = callback;
    }

    @Override
    public void getToken() {
        ThreadUtils.assertOnUiThread();

        if (isRetrievingToken()) {
            mTokenQueryTask.cancel(true);
            mTokenQueryTask = null;
        }

        if (mCallback == null) return;

        // Find the current account tracked by settings.
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(mAppContext);
        String accountName = preferences.getString(ACCOUNT_NAME_PREF, null);

        if (accountName == null || !doesAccountExist(accountName)) {
            // Remove any old preference value in case the account is invalid.
            preferences.edit().remove(ACCOUNT_NAME_PREF).commit();

            // Trigger account selection.
            mCallback.onNeedsAccountToBeSelected(getAccountChooserIntent());
            return;
        }

        mTokenQueryTask = new TokenQueryTask();
        mTokenQueryTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, accountName);
    }

    @Override
    public boolean isRetrievingToken() {
        ThreadUtils.assertOnUiThread();

        return mTokenQueryTask != null && mTokenQueryTask.getStatus() == AsyncTask.Status.RUNNING;
    }

    @Override
    public int tokenIsInvalid(String token) {
        ThreadUtils.assertOnUiThread();

        // TODO(dtrainor): Handle failure cases for tokenIsInvalid.
        try {
            GoogleAuthUtil.clearToken(mAppContext, token);
        } catch (GooglePlayServicesAvailabilityException ex) {
            return ex.getConnectionStatusCode();
        } catch (GoogleAuthException ex) {
            Log.e(TAG, "Authentication exception during token query.");
            return ConnectionResult.SIGN_IN_FAILED;
        } catch (IOException ex) {
            Log.e(TAG, "IOException during token query.");
            return ConnectionResult.SIGN_IN_FAILED;
        }
        return ConnectionResult.SUCCESS;
    }

    @Override
    public void onAccountSelected(Intent data) {
        ThreadUtils.assertOnUiThread();

        String accountName = data.getStringExtra(AccountManager.KEY_ACCOUNT_NAME);
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(mAppContext);
        preferences.edit().putString(ACCOUNT_NAME_PREF, accountName).commit();
    }

    @SuppressWarnings("deprecation")
    private Intent getAccountChooserIntent() {
        // TODO(dtrainor): Change this to com.google.android.gms.common.AccountPicker.
        return AccountManager.newChooseAccountIntent(null, null, new String[] {ACCOUNT_TYPE}, false,
                mAppContext.getString(R.string.signin_chooser_description), null, null, null);
    }

    private boolean doesAccountExist(String accountName) {
        Account[] accounts = AccountManager.get(mAppContext).getAccountsByType(ACCOUNT_TYPE);
        for (Account account : accounts) {
            if (account.name.equals(accountName)) return true;
        }

        return false;
    }

    private class TokenQueryTask extends AsyncTask<String, Void, String> {
        private final AtomicBoolean mIsRecoverable = new AtomicBoolean();

        @Override
        protected String doInBackground(String... params) {
            try {
                return GoogleAuthUtil.getTokenWithNotification(
                        mAppContext, params[0], ACCOUNT_OAUTH2_SCOPE, null);
            } catch (UserRecoverableNotifiedException ex) {
                // TODO(dtrainor): Does it make sense for this to be true if we can't recover until
                // the user performs some action?
                mIsRecoverable.set(true);
            } catch (IOException ex) {
                mIsRecoverable.set(true);
            } catch (GoogleAuthException ex) {
                mIsRecoverable.set(false);
            }
            return null;
        }

        @Override
        protected void onPostExecute(String token) {
            if (mCallback == null || isCancelled()) return;

            if (token == null) {
                mCallback.onTokenUnavailable(mIsRecoverable.get());
            } else {
                mCallback.onTokenReceived(token);
            }
            if (mTokenQueryTask == TokenQueryTask.this) mTokenQueryTask = null;
        }
    }
}