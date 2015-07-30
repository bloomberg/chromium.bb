// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.content.Context;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.sync.signin.AccountManagerHelper;

import java.io.IOException;

/**
 * ChildAccountInfoFetcher for the Android platform.
 * Checks whether an account is a child account from the AccountManager.
 */
public final class ChildAccountInfoFetcher {
    private static final String TAG = "cr.signin";

    private ChildAccountInfoFetcher() {
        // For static use only.
    }

    @CalledByNative
    private static void fetch(final long nativeAccountFetcherService, final String accountId) {
        Context app = ApplicationStatus.getApplicationContext();
        assert app != null;
        AccountManagerHelper helper = AccountManagerHelper.get(app);
        Account[] accounts = helper.getGoogleAccounts();
        Account candidate_account = null;
        for (Account account : accounts) {
            if (account.name.equals(accountId)) {
                candidate_account = account;
                break;
            }
        }
        if (candidate_account == null) {
            nativeSetIsChildAccount(nativeAccountFetcherService, accountId, false);
            return;
        }
        helper.checkChildAccount(candidate_account, new AccountManagerCallback<Boolean>() {
            @Override
            public void run(AccountManagerFuture<Boolean> future) {
                assert future.isDone();
                try {
                    boolean isChildAccount = future.getResult();
                    nativeSetIsChildAccount(nativeAccountFetcherService, accountId, isChildAccount);
                } catch (AuthenticatorException | IOException e) {
                    Log.e(TAG, "Error while fetching child account info: ", e);
                } catch (OperationCanceledException e) {
                    Log.e(TAG, "Child account info fetch was cancelled. This should not happen.");
                }
            }
        });
    }

    private static native void nativeSetIsChildAccount(
            long ptrAccountFetcherService, final String accountId, boolean isChildAccount);
}
