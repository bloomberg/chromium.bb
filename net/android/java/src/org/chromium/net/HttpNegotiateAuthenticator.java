// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Process;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
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
    private static final String TAG = "net_auth";
    private Bundle mSpnegoContext = null;
    private final String mAccountType;

    /**
     * Structure designed to hold the data related to a specific request across the various
     * callbacks needed to complete it.
     */
    static class RequestData {
        /** Native object to post the result to. */
        public long nativeResultObject;

        /** Reference to the account manager to use for the various requests. */
        public AccountManager accountManager;

        /** Authenticator-specific options for the request, used for AccountManager#getAuthToken. */
        public Bundle options;

        /** Desired token type, used for AccountManager#getAuthToken. */
        public String authTokenType;

        /** Account to fetch an auth token for. */
        public Account account;
    }

    /**
     * Expects to receive a single account as result, and uses that account to request a token
     * from the {@link AccountManager} provided via the {@link RequestData}
     */
    @VisibleForTesting
    class GetAccountsCallback implements AccountManagerCallback<Account[]> {
        private final RequestData mRequestData;
        public GetAccountsCallback(RequestData requestData) {
            mRequestData = requestData;
        }

        @Override
        public void run(AccountManagerFuture<Account[]> future) {
            Account[] accounts;
            try {
                accounts = future.getResult();
            } catch (OperationCanceledException | AuthenticatorException | IOException e) {
                Log.w(TAG, "Unable to retrieve the results for the getAccounts call.", e);
                nativeSetResult(mRequestData.nativeResultObject, NetError.ERR_UNEXPECTED, null);
                return;
            }

            if (accounts.length != 1) {
                Log.w(TAG, "Unable to obtain a unique eligible account for negotiate auth.");
                nativeSetResult(mRequestData.nativeResultObject,
                        NetError.ERR_INVALID_AUTH_CREDENTIALS, null);
                return;
            }
            if (!hasPermission(ContextUtils.getApplicationContext(),
                        "android.permission.USE_CREDENTIALS")) {
                // API  < 23 Requires the USE_CREDENTIALS permission or throws an exception.
                // API >= 23 USE_CREDENTIALS is auto-granted without having to be declared.
                Log.e(TAG, "USE_CREDENTIALS permission not granted. Aborting authentication.");
                nativeSetResult(mRequestData.nativeResultObject,
                        NetError.ERR_MISCONFIGURED_AUTH_ENVIRONMENT, null);
                return;
            }
            mRequestData.account = accounts[0];
            mRequestData.accountManager.getAuthToken(mRequestData.account,
                    mRequestData.authTokenType, mRequestData.options, true /* notifyAuthFailure */,
                    new GetTokenCallback(mRequestData),
                    new Handler(ThreadUtils.getUiThreadLooper()));
        }
    }

    @VisibleForTesting
    class GetTokenCallback implements AccountManagerCallback<Bundle> {
        private final RequestData mRequestData;
        public GetTokenCallback(RequestData requestData) {
            mRequestData = requestData;
        }

        @Override
        public void run(AccountManagerFuture<Bundle> future) {
            Bundle result;
            try {
                result = future.getResult();
            } catch (OperationCanceledException | AuthenticatorException | IOException e) {
                Log.w(TAG, "Operation did not complete", e);
                nativeSetResult(mRequestData.nativeResultObject, NetError.ERR_UNEXPECTED, null);
                return;
            }

            if (result.containsKey(AccountManager.KEY_INTENT)) {
                final Context appContext = ContextUtils.getApplicationContext();

                // We wait for a broadcast that should be sent once the user is done interacting
                // with the notification
                // TODO(dgn) We currently hang around if the notification is swiped away, until
                // a LOGIN_ACCOUNTS_CHANGED_ACTION filter is received. It might be for something
                // unrelated then we would wait again here. Maybe we should limit the number of
                // retries in some way?
                BroadcastReceiver broadcastReceiver = new BroadcastReceiver() {

                    @Override
                    public void onReceive(Context context, Intent intent) {
                        appContext.unregisterReceiver(this);
                        mRequestData.accountManager.getAuthToken(mRequestData.account,
                                mRequestData.authTokenType, mRequestData.options,
                                true /* notifyAuthFailure */, new GetTokenCallback(mRequestData),
                                null);
                    }

                };
                appContext.registerReceiver(broadcastReceiver,
                        new IntentFilter(AccountManager.LOGIN_ACCOUNTS_CHANGED_ACTION));
            } else {
                processResult(result, mRequestData);
            }
        }
    }

    protected HttpNegotiateAuthenticator(String accountType) {
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

        Context applicationContext = ContextUtils.getApplicationContext();
        RequestData requestData = new RequestData();
        requestData.authTokenType = HttpNegotiateConstants.SPNEGO_TOKEN_TYPE_BASE + principal;
        requestData.accountManager = AccountManager.get(applicationContext);
        requestData.nativeResultObject = nativeResultObject;
        String features[] = {HttpNegotiateConstants.SPNEGO_FEATURE};

        requestData.options = new Bundle();
        if (authToken != null) {
            requestData.options.putString(
                    HttpNegotiateConstants.KEY_INCOMING_AUTH_TOKEN, authToken);
        }
        if (mSpnegoContext != null) {
            requestData.options.putBundle(
                    HttpNegotiateConstants.KEY_SPNEGO_CONTEXT, mSpnegoContext);
        }
        requestData.options.putBoolean(HttpNegotiateConstants.KEY_CAN_DELEGATE, canDelegate);
        Handler uiThreadHandler = new Handler(ThreadUtils.getUiThreadLooper());

        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity == null) {
            // This code path is not as able to get user input as the one that has an activity. It
            // doesn't handle 0 or multiple accounts and will just result in a failure in those
            // cases.
            if (!hasPermission(applicationContext, android.Manifest.permission.GET_ACCOUNTS)) {
                // API  < 23 Requires the GET_ACCOUNTS permission or throws an exception.
                // API >= 23 Requires the GET_ACCOUNTS permission or returns only the accounts
                //           whose authenticator has a signature matches our app.
                Log.e(TAG, "GET_ACCOUNTS permission not granted. Aborting authentication.");
                nativeSetResult(requestData.nativeResultObject,
                        NetError.ERR_MISCONFIGURED_AUTH_ENVIRONMENT, null);
                return;
            }
            requestData.accountManager.getAccountsByTypeAndFeatures(
                    mAccountType, features, new GetAccountsCallback(requestData), uiThreadHandler);
        } else {
            // If there is more than one account, it will show the an account picker dialog for
            // each query (e.g. html file, then favicon...)
            // Same if the credentials need to be confirmed.
            // If there is a failure, it will retry automatically.
            if (!hasPermission(applicationContext, "android.permission.MANAGE_ACCOUNTS")) {
                // API  < 23 Requires the MANAGE_ACCOUNTS permission.
                // API >= 23 Requires something in the CONTACTS permission group to behave properly.
                //           MANAGE_ACCOUNTS is auto-granted on install without having be declared.
                Log.e(TAG, "MANAGE_ACCOUNTS permission not granted. Aborting authentication.");
                nativeSetResult(requestData.nativeResultObject,
                        NetError.ERR_MISCONFIGURED_AUTH_ENVIRONMENT, null);
                return;
            }
            requestData.accountManager.getAuthTokenByFeatures(mAccountType,
                    requestData.authTokenType, features, activity, null, requestData.options,
                    new GetTokenCallback(requestData), uiThreadHandler);
        }
    }

    /**
     * Process a result bundle from a completed token request, communicating its content back to
     * the native code.
     */
    private void processResult(Bundle result, RequestData requestData) {
        mSpnegoContext = result.getBundle(HttpNegotiateConstants.KEY_SPNEGO_CONTEXT);
        int status;
        switch (result.getInt(
                HttpNegotiateConstants.KEY_SPNEGO_RESULT, HttpNegotiateConstants.ERR_UNEXPECTED)) {
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
            case HttpNegotiateConstants.ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS:
                status = NetError.ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS;
                break;
            case HttpNegotiateConstants.ERR_MALFORMED_IDENTITY:
                status = NetError.ERR_MALFORMED_IDENTITY;
                break;
            default:
                status = NetError.ERR_UNEXPECTED;
        }
        nativeSetResult(requestData.nativeResultObject, status,
                result.getString(AccountManager.KEY_AUTHTOKEN));
    }

    @VisibleForTesting
    boolean hasPermission(Context context, String permission) {
        int permissionResult =
                context.checkPermission(permission, Process.myPid(), Process.myUid());
        return permissionResult == PackageManager.PERMISSION_GRANTED;
    }

    @VisibleForTesting
    native void nativeSetResult(
            long nativeJavaNegotiateResultWrapper, int status, String authToken);
}
