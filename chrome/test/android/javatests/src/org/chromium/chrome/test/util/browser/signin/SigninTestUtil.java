// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import android.accounts.Account;
import android.app.Instrumentation;
import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.init.ProcessInitializationHandler;
import org.chromium.chrome.browser.signin.AccountIdProvider;
import org.chromium.chrome.browser.signin.AccountTrackerService;
import org.chromium.chrome.browser.signin.OAuth2TokenService;
import org.chromium.components.signin.AccountManagerHelper;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.MockAccountManager;

import java.util.HashSet;

/**
 * Utility class for test signin functionality.
 */
public final class SigninTestUtil {
    private static final String TAG = "Signin";

    private static final String DEFAULT_ACCOUNT = "test@gmail.com";

    private static Context sContext;
    private static MockAccountManager sAccountManager;

    /**
     * Sets up the test authentication environment.
     *
     * This must be called before native is loaded.
     */
    public static void setUpAuthForTest(Instrumentation instrumentation) {
        assert sContext == null;
        sContext = instrumentation.getTargetContext();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ProcessInitializationHandler.getInstance().initializePreNative();
            }
        });
        sAccountManager = new MockAccountManager(sContext, instrumentation.getContext());
        AccountManagerHelper.overrideAccountManagerHelperForTests(sContext, sAccountManager);
        overrideAccountIdProvider();
        resetSigninState();
    }

    /**
     * Returns the currently signed in account.
     */
    public static Account getCurrentAccount() {
        assert sContext != null;
        return ChromeSigninController.get(sContext).getSignedInUser();
    }

    /**
     * Add an account with the default name.
     */
    public static Account addTestAccount() {
        return addTestAccount(DEFAULT_ACCOUNT);
    }

    /**
     * Add an account with a given name.
     */
    public static Account addTestAccount(String name) {
        Account account = createTestAccount(name);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                AccountTrackerService.get(sContext).invalidateAccountSeedStatus(true);
            }
        });
        return account;
    }

    /**
     * Add and sign in an account with the default name.
     */
    public static Account addAndSignInTestAccount() {
        Account account = createTestAccount(DEFAULT_ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ChromeSigninController.get(sContext).setSignedInAccountName(DEFAULT_ACCOUNT);
                AccountTrackerService.get(sContext).invalidateAccountSeedStatus(true);
            }
        });
        return account;
    }

    private static Account createTestAccount(String accountName) {
        assert sContext != null;
        Account account = AccountManagerHelper.createAccountFromName(accountName);
        AccountHolder.Builder accountHolder =
                AccountHolder.create().account(account).alwaysAccept(true);
        sAccountManager.addAccountHolderExplicitly(accountHolder.build());
        return account;
    }

    private static void overrideAccountIdProvider() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                AccountIdProvider.setInstanceForTest(new AccountIdProvider() {
                    @Override
                    public String getAccountId(Context ctx, String accountName) {
                        return "gaia-id-" + accountName;
                    }

                    @Override
                    public boolean canBeUsed(Context ctx) {
                        return true;
                    }
                });
            }
        });
    }

    /**
     * Should be called at setUp and tearDown so that the signin state is not leaked across tests.
     * The setUp call is implicit inside the constructor.
     */
    public static void resetSigninState() {
        // Clear cached signed account name and accounts list.
        ChromeSigninController.get(sContext).setSignedInAccountName(null);
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putStringSet(OAuth2TokenService.STORED_ACCOUNTS_KEY, new HashSet<String>())
                .apply();
    }

    private SigninTestUtil() {}
}
