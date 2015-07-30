// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.IOException;

/**
 * Class to get Auth Tokens for HTTP Negotiate authentication (typically used for Kerberos) An
 * instance of this class is created for each separate negotiation.
 */
@JNINamespace("net::android")
public class HttpNegotiateAuthenticator {
    private Bundle mSpnegoContext = null;
    private final String mAccountType;
    private AccountManagerFuture<Bundle> mFuture;

    private HttpNegotiateAuthenticator(String accountType) {
        assert !android.text.TextUtils.isEmpty(accountType);
        mAccountType = accountType;
    }

    /**
     * @param accountType The Android account type to use.
     */
    @VisibleForTesting
    @CalledByNative
    static HttpNegotiateAuthenticator create(String accountType) {
        return new HttpNegotiateAuthenticator(accountType);
    }

    /**
     * @param nativeResultObject The C++ object used to return the result. For correct C++ memory
     *            management we must call nativeSetResult precisely once with this object.
     * @param principal The principal (must be host based).
     * @param authToken The incoming auth token.
     * @param canDelegate True if we can delegate.
     */
    @VisibleForTesting
    @CalledByNative
    void getNextAuthToken(final long nativeResultObject, final String principal, String authToken,
            boolean canDelegate) {
        assert principal != null;
        String authTokenType = HttpNegotiateConstants.SPNEGO_TOKEN_TYPE_BASE + principal;
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity == null) {
            nativeSetResult(nativeResultObject, NetError.ERR_UNEXPECTED, null);
            return;
        }
        AccountManager am = AccountManager.get(activity);
        String features[] = {HttpNegotiateConstants.SPNEGO_FEATURE};

        Bundle options = new Bundle();

        if (authToken != null) {
            options.putString(HttpNegotiateConstants.KEY_INCOMING_AUTH_TOKEN, authToken);
        }
        if (mSpnegoContext != null) {
            options.putBundle(HttpNegotiateConstants.KEY_SPNEGO_CONTEXT, mSpnegoContext);
        }
        options.putBoolean(HttpNegotiateConstants.KEY_CAN_DELEGATE, canDelegate);

        mFuture = am.getAuthTokenByFeatures(mAccountType, authTokenType, features, activity, null,
                options, new AccountManagerCallback<Bundle>() {

                    @Override
                    public void run(AccountManagerFuture<Bundle> future) {
                        try {
                            Bundle result = future.getResult();
                            mSpnegoContext =
                                    result.getBundle(HttpNegotiateConstants.KEY_SPNEGO_CONTEXT);
                            int status;
                            switch (result.getInt(HttpNegotiateConstants.KEY_SPNEGO_RESULT,
                                    HttpNegotiateConstants.ERR_UNEXPECTED)) {
                                case HttpNegotiateConstants.OK:
                                    status = 0;
                                    break;
                                case HttpNegotiateConstants.ERR_UNEXPECTED:
                                    status = NetError.ERR_UNEXPECTED;
                                    break;
                                case HttpNegotiateConstants.ERR_ABORTED:
                                    status = NetError.ERR_ABORTED;
                                    break;
                                case HttpNegotiateConstants.ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS:
                                    status = NetError.ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
                                    break;
                                case HttpNegotiateConstants.ERR_INVALID_RESPONSE:
                                    status = NetError.ERR_INVALID_RESPONSE;
                                    break;
                                case HttpNegotiateConstants.ERR_INVALID_AUTH_CREDENTIALS:
                                    status = NetError.ERR_INVALID_AUTH_CREDENTIALS;
                                    break;
                                case HttpNegotiateConstants.ERR_UNSUPPORTED_AUTH_SCHEME:
                                    status = NetError.ERR_UNSUPPORTED_AUTH_SCHEME;
                                    break;
                                case HttpNegotiateConstants.ERR_MISSING_AUTH_CREDENTIALS:
                                    status = NetError.ERR_MISSING_AUTH_CREDENTIALS;
                                    break;
                                case HttpNegotiateConstants
                                        .ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS:
                                    status = NetError.ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS;
                                    break;
                                case HttpNegotiateConstants.ERR_MALFORMED_IDENTITY:
                                    status = NetError.ERR_MALFORMED_IDENTITY;
                                    break;
                                default:
                                    status = NetError.ERR_UNEXPECTED;
                            }
                            nativeSetResult(nativeResultObject, status,
                                    result.getString(AccountManager.KEY_AUTHTOKEN));
                        } catch (OperationCanceledException | AuthenticatorException
                                | IOException e) {
                            nativeSetResult(nativeResultObject, NetError.ERR_ABORTED, null);
                        }
                    }

                }, new Handler(ThreadUtils.getUiThreadLooper()));
    }

    @VisibleForTesting
    native void nativeSetResult(
            long nativeJavaNegotiateResultWrapper, int status, String authToken);
}
