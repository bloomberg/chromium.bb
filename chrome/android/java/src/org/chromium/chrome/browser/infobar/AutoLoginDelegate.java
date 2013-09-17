// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.accounts.Account;
import android.app.Activity;
import android.util.Log;
import android.util.SparseArray;

import org.chromium.chrome.R;

import org.chromium.base.CalledByNative;

/**
 * Java equivalent of the C++ AutoLoginDelegateAndroid.
 *
 * Offers functionality to log in using the account in the system and keeps track
 * of all the native autologin infobars and their respective accounts.
 */
public class AutoLoginDelegate {
    private final Activity mActivity;
    private final AutoLoginProcessor mAutoLoginProcessor;

    // indexed by nativeInfoBar, think of it as an efficient
    // HashMap<nativeInfoBar, AutoLoginAccountDelegate>.
    private final SparseArray<AutoLoginAccountDelegate> mAccountHelpers;

    public AutoLoginDelegate(AutoLoginProcessor autoLoginProcessor, Activity activity) {
        mActivity = activity;
        mAutoLoginProcessor = autoLoginProcessor;
        mAccountHelpers = new SparseArray<AutoLoginAccountDelegate>();
    }

    /**
     * @return the account name of the device if any.
     */
    @CalledByNative
    String initializeAccount(int nativeInfoBar, String realm, String account, String args) {
        AutoLoginAccountDelegate accountHelper =
                new AutoLoginAccountDelegate(mActivity, mAutoLoginProcessor, realm, account, args);

        if (!accountHelper.hasAccount()) {
            return "";
        }

        mAccountHelpers.put(nativeInfoBar, accountHelper);
        return accountHelper.getAccountName();
    }

    /**
     * Log in a user to a given google service.
     */
    @CalledByNative
    boolean logIn(int nativeInfoBar) {
        AutoLoginAccountDelegate account = mAccountHelpers.get(nativeInfoBar);

        if (account == null || !account.logIn()) {
            nativeLoginFailed(nativeInfoBar);
            return false;
        }
        return true;
    }

    /**
     * Clear account information for cancelled login requests.
     */
    @CalledByNative
    boolean cancelLogIn(int nativeInfoBar) {
        mAccountHelpers.remove(nativeInfoBar);
        return true;
    }

    /**
     * Clear all infobars in the same tab and tells native to proceed with login if successful.
     */
    public void dismissAutoLogins(String accountName, String authToken, boolean success,
            String result) {
        // Use a copy of the current active accounts so we can delete the
        // original in the for loop.
        SparseArray<AutoLoginAccountDelegate> accountInfo = mAccountHelpers.clone();
        for (int i = 0; i < accountInfo.size(); i++) {
            int infoBar = accountInfo.keyAt(i);
            AutoLoginAccountDelegate delegate = accountInfo.get(infoBar);
            if (!delegate.loginRequested()) {
                nativeLoginDismiss(infoBar);
            } else {
                String accountAuthToken = delegate.getAuthToken();
                if (accountAuthToken != null && accountAuthToken.equals(authToken)
                        && delegate.loginRequested()) {
                    if (success) {
                        nativeLoginSuccess(infoBar, result);
                    } else {
                        nativeLoginFailed(infoBar);
                    }
                }
            }
            mAccountHelpers.remove(infoBar);
        }
    }

    private native void nativeLoginSuccess(int nativeAutoLoginInfoBarDelegateAndroid,
            String result);
    private native void nativeLoginFailed(int nativeAutoLoginInfoBarDelegateAndroid);
    private native void nativeLoginDismiss(int nativeAutoLoginInfoBarDelegateAndroid);
}
