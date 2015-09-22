// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.childaccounts;

import android.accounts.Account;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.content.Context;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.sync.signin.AccountManagerHelper;

import java.io.IOException;

/**
 * This class serves as a simple interface for querying the child account information.
 * It has two methods namely, checkHasChildAccount(...) which is asynchronous and queries the
 * system directly for the information and the synchronous isChildAccount() which asks the native
 * side assuming it has been set correctly already.
 *
 * The former method is used by ForcedSigninProcessor and FirstRunFlowSequencer to detect child
 * accounts since the native side is only activated on signing in.
 * Once signed in by the ForcedSigninProcessor, the ChildAccountInfoFetcher will notify the native
 * side and also takes responsibility for monitoring changes and taking a suitable action.
 */
public class ChildAccountService {
    private static final String TAG = "ChildAccountService";

    private ChildAccountService() {
        // Only for static usage.
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
     *
     * @param callback A callback which will be called with the result.
     */
    public static void checkHasChildAccount(
            Context context, final HasChildAccountCallback callback) {
        ThreadUtils.assertOnUiThread();
        if (!nativeIsChildAccountDetectionEnabled()) {
            callback.onChildAccountChecked(false);
            return;
        }
        AccountManagerHelper helper = AccountManagerHelper.get(context);
        Account[] accounts = helper.getGoogleAccounts();
        if (accounts.length != 1) {
            callback.onChildAccountChecked(false);
            return;
        }
        helper.checkChildAccount(accounts[0], new AccountManagerCallback<Boolean>() {
            @Override
            public void run(AccountManagerFuture<Boolean> future) {
                assert future.isDone();
                boolean hasFeatures = false;
                try {
                    hasFeatures = future.getResult();
                } catch (AuthenticatorException | IOException e) {
                    Log.e(TAG, "Error while checking features: ", e);
                } catch (OperationCanceledException e) {
                    Log.e(TAG, "Checking features was cancelled. This should not happen.");
                }
                callback.onChildAccountChecked(hasFeatures);
            }
        });
    }

    /**
     * Returns the previously determined value of whether there is a child account on the device.
     * Should only be called after the native library and profile have been loaded.
     *
     * @return The previously determined value of whether there is a child account on the device.
     */
    public static boolean isChildAccount() {
        return nativeIsChildAccount();
    }

    private static native boolean nativeIsChildAccount();

    /**
     * If this returns false, Chrome will assume there are no child accounts on the device,
     * and no further checks will be made, which has the effect of a kill switch.
     */
    private static native boolean nativeIsChildAccountDetectionEnabled();
}
