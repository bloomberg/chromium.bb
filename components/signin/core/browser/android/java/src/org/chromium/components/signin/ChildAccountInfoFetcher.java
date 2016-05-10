// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;
import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.sync.signin.AccountManagerHelper;

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
    private static void fetch(final long nativeAccountFetcherService, final String accountId,
            final String accountName) {
        Context app = ContextUtils.getApplicationContext();
        assert app != null;
        AccountManagerHelper helper = AccountManagerHelper.get(app);
        Account account = helper.createAccountFromName(accountName);
        helper.checkChildAccount(account, new Callback<Boolean>() {
            @Override
            public void onResult(Boolean isChildAccount) {
                nativeSetIsChildAccount(nativeAccountFetcherService, accountId, isChildAccount);
            }
        });
    }

    private static native void nativeSetIsChildAccount(
            long ptrAccountFetcherService, final String accountId, boolean isChildAccount);
}
