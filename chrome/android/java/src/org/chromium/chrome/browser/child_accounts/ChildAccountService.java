// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.child_accounts;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.content.Context;
import android.os.AsyncTask;
import android.util.Log;

import org.chromium.base.CommandLine;
import org.chromium.chrome.ChromeSwitches;
import org.chromium.sync.signin.AccountManagerHelper;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Timer;
import java.util.TimerTask;
import java.util.concurrent.ExecutionException;

/**
 * This class detects child accounts and enables special treatment for them.
 */
public class ChildAccountService {

    private static final String TAG = "ChildAccountService";

    private static final int CHILD_ACCOUNT_DONT_FORCE = 0;
    private static final int CHILD_ACCOUNT_FORCE_ON = 1;
    private static final int CHILD_ACCOUNT_FORCE_OFF = 2;

    /**
     * An account feature (corresponding to a Gaia service flag) that specifies whether the account
     * is a child account.
     */
    private static final String FEATURE_IS_CHILD_ACCOUNT_KEY = "service_uca";

    /**
     * The maximum amount of time to wait for the child account check, in milliseconds.
     */
    private static final int CHILD_ACCOUNT_TIMEOUT_MS = 1000;

    private static final Object sLock = new Object();

    private static ChildAccountService sChildAccountService;

    private final Context mContext;

    // This is null if we haven't determined the child account status yet.
    private Boolean mHasChildAccount;

    private AccountManagerFuture<Boolean> mAccountManagerFuture;

    private final List<HasChildAccountCallback> mCallbacks = new ArrayList<>();

    private ChildAccountService(Context context) {
        mContext = context;
    }

    /**
     * Returns the shared ChildAccountService instance, creating one if necessary.
     * @param context The context to initialize the ChildAccountService with.
     * @return The shared instance.
     */
    public static ChildAccountService getInstance(Context context) {
        synchronized (sLock) {
            if (sChildAccountService == null) {
                sChildAccountService = new ChildAccountService(context.getApplicationContext());
            }
        }
        return sChildAccountService;
    }

    /**
     * A callback to return the result of {@link #checkHasChildAccount}.
     */
    public static interface HasChildAccountCallback {

        /**
         * @param hasChildAccount Whether there is exactly one child account on the device.
         */
        public void onChildAccountChecked(boolean hasChildAccount);

    }

    /**
     * Checks for the presence of child accounts on the device.
     * @param callback Will be called with the result (see
     * {@link HasChildAccountCallback#onChildAccountChecked}).
     */
    public void checkHasChildAccount(final HasChildAccountCallback callback) {
        if (mHasChildAccount != null) {
            callback.onChildAccountChecked(mHasChildAccount);
            return;
        }

        Account[] googleAccounts =
                AccountManagerHelper.get(mContext).getGoogleAccounts();
        if (googleAccounts.length != 1) {
            mHasChildAccount = false;
            callback.onChildAccountChecked(false);
            if (CommandLine.getInstance().hasSwitch(ChromeSwitches.CHILD_ACCOUNT)) {
                Log.w(TAG, "Ignoring --" + ChromeSwitches.CHILD_ACCOUNT + " command line flag "
                        + "because there are " + googleAccounts.length + " Google accounts on the "
                        + "device");
            }
            return;
        }
        Account account = googleAccounts[0];

        int forceChildAccount = shouldForceChildAccount(account);
        if (forceChildAccount != CHILD_ACCOUNT_DONT_FORCE) {
            mHasChildAccount = forceChildAccount == CHILD_ACCOUNT_FORCE_ON;
            callback.onChildAccountChecked(mHasChildAccount);
            return;
        }

        mCallbacks.add(callback);

        if (mAccountManagerFuture != null) return;

        final Timer timer = new Timer();

        String[] features = {FEATURE_IS_CHILD_ACCOUNT_KEY};
        mAccountManagerFuture = AccountManager.get(mContext).hasFeatures(account, features,
                new AccountManagerCallback<Boolean>() {
                    @Override
                    public void run(AccountManagerFuture<Boolean> future) {
                        Log.i(TAG, "completed AM request");
                        assert future == mAccountManagerFuture;
                        assert future.isDone();

                        timer.cancel();

                        boolean hasChildAccount = hasChildAccount();
                        for (HasChildAccountCallback callback : mCallbacks) {
                            callback.onChildAccountChecked(hasChildAccount);
                        }
                    }
                }, null /* handler */);

        timer.schedule(new TimerTask() {
            @Override
            public void run() {
                if (!mAccountManagerFuture.isDone()) {
                    Log.i(TAG, "cancelling AM request");
                    mAccountManagerFuture.cancel(true);
                }
            }}, CHILD_ACCOUNT_TIMEOUT_MS);
    }

    private int shouldForceChildAccount(Account account) {
        String childAccountName = CommandLine.getInstance().getSwitchValue(
                ChromeSwitches.CHILD_ACCOUNT);
        if (childAccountName != null && account.name.equals(childAccountName)) {
            return CHILD_ACCOUNT_FORCE_ON;
        }
        return CHILD_ACCOUNT_DONT_FORCE;
    }

    private boolean getFutureResult() {
        try {
            Log.i(TAG, "before mAccountManagerFuture.getResult()");
            boolean result = mAccountManagerFuture.getResult();
            Log.i(TAG, "after mAccountManagerFuture.getResult()");
            return result;
        } catch (OperationCanceledException e) {
            Log.e(TAG, "Timed out fetching child account flag: ", e);
        } catch (AuthenticatorException e) {
            Log.e(TAG, "Error while fetching child account flag: ", e);
        } catch (IOException e) {
            Log.e(TAG, "Error while fetching child account flag: ", e);
        }
        return false;
    }

    /**
     * Synchronously checks for the presence of child accounts on the device. This method should
     * only be called after the result has been determined (usually using
     * {@link #checkHasChildAccount} or {@link #waitUntilFinished} to block).
     * @return Whether there is a child account on the device.
     */
    public boolean hasChildAccount() {
        // Lazily get the result from the future, so this can be called both from the
        // AccountManagerCallback and after waiting for the future to be resolved.
        if (mHasChildAccount == null) {
            // If the future is not resolved yet, this will assert.
            mHasChildAccount = getFutureResult();
        }
        return mHasChildAccount;
    }

    /**
     * Waits until we have determined the child account status. Usually you should use callbacks
     * instead of this method, see {@link #checkHasChildAccount}.
     */
    public void waitUntilFinished() {
        if (mAccountManagerFuture == null) return;
        if (mAccountManagerFuture.isDone()) return;

        // This will block in getFutureResult(), but that may only happen on a background thread.
        try {
            new AsyncTask<Void, Void, Void>() {
                @Override
                protected Void doInBackground(Void... params) {
                    getFutureResult();
                    return null;
                }
            }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR).get();
        } catch (ExecutionException e) {
            Log.w(TAG, "Error while fetching child account flag: ", e);
        } catch (InterruptedException e) {
            Log.w(TAG, "Interrupted while fetching child account flag: ", e);
        }
    }

    private boolean isChildAccountDetectionEnabled() {
        return nativeIsChildAccountDetectionEnabled();
    }

    public void onChildAccountSigninComplete() {
        nativeOnChildAccountSigninComplete();
    }

    private native boolean nativeIsChildAccountDetectionEnabled();
    private native void nativeOnChildAccountSigninComplete();
}
