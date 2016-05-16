// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.accounts.Account;
import android.annotation.TargetApi;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.preference.PreferenceManager;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.signin.AccountIdProvider;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.sync.test.util.MockAccountManager;

/**
 * Android backup tests.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
@CommandLineFlags.Remove({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ChromeBackupIntegrationTest extends ChromeTabbedActivityTestBase {

    private static final String GOOGLE_ACCOUNT_TYPE = "com.google";

    @Override
    public void startMainActivity() throws InterruptedException {
        // Do nothing here, the tests need to do some per-test setup before they start the main
        // activity.
    }

    private static final class MockAccountIdProvider extends AccountIdProvider {
        @Override
        public String getAccountId(Context ctx, String accountName) {
            return accountName;
        }

        @Override
        public boolean canBeUsed(Context ctx) {
            return true;
        }
    }

    static class ChromeTestBackupAgent extends ChromeBackupAgent {
        ChromeTestBackupAgent(Context context) {
            // This is protected in ContextWrapper, so can only be called within a derived
            // class.
            attachBaseContext(context);
        }
    }

    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    public void testSimpleRestore() throws InterruptedException {
        Context targetContext = getInstrumentation().getTargetContext();

        // Fake having previously gone through FRE and signed in.
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(targetContext);
        SharedPreferences.Editor preferenceEditor = prefs.edit();
        preferenceEditor.putBoolean(FirstRunStatus.FIRST_RUN_FLOW_COMPLETE, true);
        preferenceEditor.putBoolean(FirstRunSignInProcessor.FIRST_RUN_FLOW_SIGNIN_SETUP, true);
        preferenceEditor.putString(ChromeSigninController.SIGNED_IN_ACCOUNT_KEY, "user1@gmail.com");
        preferenceEditor.commit();

        Account account = new Account("user1@gmail.com", GOOGLE_ACCOUNT_TYPE);
        MockAccountManager accountManager =
                new MockAccountManager(targetContext, getInstrumentation().getContext(), account);
        AccountManagerHelper.overrideAccountManagerHelperForTests(targetContext, accountManager);
        AccountIdProvider.setInstanceForTest(new MockAccountIdProvider());

        // Run Chrome's restore code.
        new ChromeTestBackupAgent(targetContext).onRestoreFinished();

        // Start Chrome and check that it signs in.
        startMainActivityFromLauncher();

        assertTrue(ChromeSigninController.get(targetContext).isSignedIn());
        assertEquals("user1@gmail.com",
                ChromeSigninController.get(targetContext).getSignedInAccountName());
    }

    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    public void testRestoreAccountMissing() throws InterruptedException {
        Context targetContext = getInstrumentation().getTargetContext();

        // Fake having previously gone through FRE and signed in.
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(targetContext);
        SharedPreferences.Editor preferenceEditor = prefs.edit();
        preferenceEditor.putBoolean(FirstRunStatus.FIRST_RUN_FLOW_COMPLETE, true);
        preferenceEditor.putBoolean(FirstRunSignInProcessor.FIRST_RUN_FLOW_SIGNIN_SETUP, true);
        preferenceEditor.putString(ChromeSigninController.SIGNED_IN_ACCOUNT_KEY, "user1@gmail.com");
        preferenceEditor.commit();

        // Create a mock account manager with a different account
        Account account = new Account("user2@gmail.com", GOOGLE_ACCOUNT_TYPE);
        MockAccountManager accountManager =
                new MockAccountManager(targetContext, getInstrumentation().getContext(), account);
        AccountManagerHelper.overrideAccountManagerHelperForTests(targetContext, accountManager);
        AccountIdProvider.setInstanceForTest(new MockAccountIdProvider());

        // Run Chrome's restore code.
        new ChromeTestBackupAgent(targetContext).onRestoreFinished();

        // Start Chrome.
        startMainActivityFromLauncher();

        // Since the account didn't exist, Chrome should not be signed in.
        assertFalse(ChromeSigninController.get(targetContext).isSignedIn());
    }

}
