// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell.signin;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.OAuth2TokenService;
import org.chromium.content.browser.BrowserStartupController;
import org.chromium.sync.signin.ChromeSigninController;

/**
 * A BroadcastReceiver for acting on changes to Android accounts.
 */
public class AccountsChangedReceiver extends BroadcastReceiver {
    private static final String TAG = "AccountsChangedReceiver";

    @Override
    public void onReceive(final Context context, Intent intent) {
        if (AccountManager.LOGIN_ACCOUNTS_CHANGED_ACTION.equals(intent.getAction())) {
            final Account signedInUser =
                    ChromeSigninController.get(context).getSignedInUser();
            if (signedInUser != null) {
                BrowserStartupController.StartupCallback callback =
                        new BrowserStartupController.StartupCallback() {
                    @Override
                    public void onSuccess(boolean alreadyStarted) {
                        OAuth2TokenService.getForProfile(Profile.getLastUsedProfile())
                                .validateAccounts(context, false);
                    }

                    @Override
                    public void onFailure() {
                        Log.w(TAG, "Failed to start browser process.");
                    }
                };
                startBrowserProcessOnUiThread(context, callback);
            }
        }
    }

    private static void startBrowserProcessOnUiThread(final Context context,
            final BrowserStartupController.StartupCallback callback) {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    BrowserStartupController.get(context).startBrowserProcessesAsync(callback);
                }
                catch (ProcessInitException e) {
                    Log.e(TAG, "Unable to load native library.", e);
                    System.exit(-1);
                }
            }
        });
    }
}
