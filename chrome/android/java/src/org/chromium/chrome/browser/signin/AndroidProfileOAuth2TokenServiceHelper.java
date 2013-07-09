// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.content.Context;
import android.util.Log;

import com.google.common.annotations.VisibleForTesting;

import org.chromium.base.CalledByNative;
import org.chromium.base.NativeClassQualifiedName;
import org.chromium.base.ThreadUtils;
import org.chromium.sync.signin.AccountManagerHelper;

/**
 * Helper class for working with access tokens from native code.
 * <p/>
 * This class forwards calls to request or invalidate access tokens made by native code to
 * AccountManagerHelper and forwards callbacks to native code.
 * <p/>
 */
public class AndroidProfileOAuth2TokenServiceHelper {

    private static final String TAG = "AndroidProfileOAuth2TokenServiceHelper";

    private static final String OAUTH2_SCOPE_PREFIX = "oauth2:";

    private static Account getAccountOrNullFromUsername(Context context, String username) {
        if (username == null) {
            Log.e(TAG, "Username is null");
            return null;
        }

        AccountManagerHelper accountManagerHelper = AccountManagerHelper.get(context);
        Account account = accountManagerHelper.getAccountFromName(username);
        if (account == null) {
            Log.e(TAG, "Account not found for provided username.");
            return null;
        }
        return account;
    }

    /**
     * Called by native to retrieve OAuth2 tokens.
     *
     * @param username The native username (full address).
     * @param scope The scope to get an auth token for (without Android-style 'oauth2:' prefix).
     * @param oldAuthToken If provided, the token will be invalidated before getting a new token.
     * @param nativeCallback The pointer to the native callback that should be run upon completion.
     */
    @CalledByNative
    public static void getOAuth2AuthToken(
            Context context, String username, String scope, final int nativeCallback) {
        Account account = getAccountOrNullFromUsername(context, username);
        if (account == null) {
            nativeOAuth2TokenFetched(null, false, nativeCallback);
            return;
        }
        String oauth2Scope = OAUTH2_SCOPE_PREFIX + scope;

        AccountManagerHelper accountManagerHelper = AccountManagerHelper.get(context);
        accountManagerHelper.getAuthTokenFromForeground(
            null, account, oauth2Scope, new AccountManagerHelper.GetAuthTokenCallback() {
                @Override
                public void tokenAvailable(String token) {
                    nativeOAuth2TokenFetched(
                        token, token != null, nativeCallback);
                }
            });
    }

   /**
    * Called by native to invalidate an OAuth2 token.
    */
    @CalledByNative
    public static void invalidateOAuth2AuthToken(
            Context context, String accessToken) {
        AccountManagerHelper.get(context).invalidateAuthToken(accessToken);
    }

    private static native void nativeOAuth2TokenFetched(
            String authToken, boolean result, int nativeCallback);

}
