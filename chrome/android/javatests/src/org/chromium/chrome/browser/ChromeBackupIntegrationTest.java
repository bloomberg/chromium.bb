// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.accounts.Account;
import android.annotation.TargetApi;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ContextUtils;
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
    private static final String TEST_ACCOUNT_1 = "user1@gmail.com";
    private static final String TEST_ACCOUNT_2 = "user2@gmail.com";
    private Context mTargetContext;

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

        @Override
        protected Account[] getAccounts() {
            // ChromeBackupAgent can't use Chrome's account manager, so we override this to mock
            // the existence of the account.
            return new Account[]{new Account(TEST_ACCOUNT_1, GOOGLE_ACCOUNT_TYPE)};
        }

    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        clearAppData();

        mTargetContext = getInstrumentation().getTargetContext();

        // Create a mock account manager. Although this isn't used by ChromeBackupAgent it is used
        // when we test the result by starting Chrome.
        ChromeBackupAgent.allowChromeApplicationForTesting();
        Account account = new Account(TEST_ACCOUNT_1, GOOGLE_ACCOUNT_TYPE);
        MockAccountManager accountManager =
                new MockAccountManager(mTargetContext, getInstrumentation().getContext(), account);
        AccountManagerHelper.overrideAccountManagerHelperForTests(mTargetContext, accountManager);
        AccountIdProvider.setInstanceForTest(new MockAccountIdProvider());
    }

    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    public void testSimpleRestore() throws InterruptedException {

        // Fake having previously gone through FRE and signed in.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor preferenceEditor = prefs.edit();
        preferenceEditor.putBoolean(FirstRunStatus.FIRST_RUN_FLOW_COMPLETE, true);
        preferenceEditor.putBoolean(FirstRunSignInProcessor.FIRST_RUN_FLOW_SIGNIN_SETUP, true);

        // Set up the mocked account as the signed in account.
        preferenceEditor.putString(ChromeSigninController.SIGNED_IN_ACCOUNT_KEY, TEST_ACCOUNT_1);
        preferenceEditor.commit();

        // Run Chrome's restore code.
        new ChromeTestBackupAgent(mTargetContext).onRestoreFinished();

        // Start Chrome and check that it signs in.
        startMainActivityFromLauncher();

        assertTrue(ChromeSigninController.get(mTargetContext).isSignedIn());
        assertEquals(TEST_ACCOUNT_1,
                ChromeSigninController.get(mTargetContext).getSignedInAccountName());
    }

    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    public void testRestoreAccountMissing() throws InterruptedException {
        // Fake having previously gone through FRE and signed in.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor preferenceEditor = prefs.edit();
        preferenceEditor.putBoolean(FirstRunStatus.FIRST_RUN_FLOW_COMPLETE, true);
        preferenceEditor.putBoolean(FirstRunSignInProcessor.FIRST_RUN_FLOW_SIGNIN_SETUP, true);

        // Use an account that hasn't been created by the mocks as the signed in account.
        preferenceEditor.putString(ChromeSigninController.SIGNED_IN_ACCOUNT_KEY, TEST_ACCOUNT_2);
        preferenceEditor.commit();

        // Run Chrome's restore code.
        new ChromeTestBackupAgent(mTargetContext).onRestoreFinished();

        // Start Chrome.
        startMainActivityFromLauncher();

        // Since the account didn't exist, Chrome should not be signed in.
        assertFalse(ChromeSigninController.get(mTargetContext).isSignedIn());
    }

}
