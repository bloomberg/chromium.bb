// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.accounts.Account;
import android.content.Context;

import org.chromium.base.JNINamespace;
import org.chromium.sync.signin.AccountManagerHelper;

import java.util.List;

/**
* Wallet automatic sign-in helper for AutofillDialog.
*/
@JNINamespace("autofill")
public class AutofillDialogAccountHelper {
    private static final String TAG = "AutofillDialogAccountHelper";
    private static int MAX_ATTEMPTS_TO_GENERATE_TOKENS = 2;

    private final Context mContext;
    private final SignInContinuation mSignInContinuation;

    private Account mAccount;
    private String mSid;
    private String mLsid;
    private int mAttemptsToGo;

    /**
     * Delegate that will be called on the completion of tokens generation.
     */
    public static interface SignInContinuation {
        /**
         * Tokens generation has succeeded.
         * @param accountName The account name.
         * @param sid A GAIA session authentication SID token.
         * @param lsid An SSL-only GAIA authentication LSID token.
         */
        void onTokensGenerationSuccess(String accountName, String sid, String lsid);

        /**
         * Tokens generation has failed.
         */
        void onTokensGenerationFailure();
    }

    public AutofillDialogAccountHelper(SignInContinuation continuation, Context context) {
        mSignInContinuation = continuation;
        mContext = context;
    }

    /**
     * @return An array of Google account emails the user has.
     */
    public String[] getAccountNames() {
        List<String> accountNames = AccountManagerHelper.get(mContext).getGoogleAccountNames();
        return accountNames.toArray(new String[accountNames.size()]);
    }

    /**
     * Starts generation of tokens.
     * This results in onTokensGenerationSuccess/onTokensGenerationFailure call.
     * @param accountName An account name.
     */
    public void startTokensGeneration(String accountName) {
        mAccount = AccountManagerHelper.get(mContext).getAccountFromName(accountName);
        if (mAccount == null) {
            mSignInContinuation.onTokensGenerationFailure();
            return;
        }
        mAttemptsToGo = MAX_ATTEMPTS_TO_GENERATE_TOKENS;
        attemptToGenerateTokensOrRetryOrFail();
    }

    private void startTokensGenerationImpl() {
        AccountManagerHelper.get(mContext).getNewAuthTokenFromForeground(
                mAccount, mSid, "SID",
                new AccountManagerHelper.GetAuthTokenCallback() {
                    @Override
                    public void tokenAvailable(String authToken) {
                        mSid = authToken;
                        if (mSid == null) {
                            attemptToGenerateTokensOrRetryOrFail();
                            return;
                        }
                        AccountManagerHelper.get(mContext).getNewAuthTokenFromForeground(
                                mAccount, mLsid, "LSID",
                                new AccountManagerHelper.GetAuthTokenCallback() {
                                    @Override
                                    public void tokenAvailable(String authToken) {
                                        mLsid = authToken;
                                        if (mLsid == null) {
                                            attemptToGenerateTokensOrRetryOrFail();
                                            return;
                                        }
                                        mSignInContinuation.onTokensGenerationSuccess(
                                                mAccount.name, mSid, mLsid);
                                    }
                                });
                    }
                });
    }

    private void attemptToGenerateTokensOrRetryOrFail() {
        assert mAttemptsToGo > 0;
        --mAttemptsToGo;
        if (mAttemptsToGo > 0) {
            startTokensGenerationImpl();
        } else {
            mSignInContinuation.onTokensGenerationFailure();
        }
    }
}
