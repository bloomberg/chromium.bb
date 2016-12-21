// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.os.Bundle;
import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.firstrun.FirstRunActivity;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.signin.AccountManagementFragment;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Tests for the first run experience.
 */
@CommandLineFlags.Remove(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@RetryOnFailure  // crbug.com/637448
public class FirstRunTest extends SyncTestBase {
    private static final String TAG = "FirstRunTest";

    private enum ShowSettings {
        YES,
        NO;
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityFromLauncher();
    }

    // Test that signing in through FirstRun signs in and starts sync.
    /*
     * @SmallTest
     * @Feature({"Sync"})
     */
    @FlakyTest(message = "https://crbug.com/616456")
    public void testSignIn() throws Exception {
        Account testAccount = SigninTestUtil.addTestAccount();
        assertNull(SigninTestUtil.getCurrentAccount());
        assertFalse(SyncTestUtil.isSyncRequested());

        processFirstRun(testAccount.name, ShowSettings.NO);
        assertEquals(testAccount, SigninTestUtil.getCurrentAccount());
        SyncTestUtil.waitForSyncActive();
    }

    // Test that signing in and opening settings through FirstRun signs in and doesn't fully start
    // sync until the settings page is closed.
    /*
     * @SmallTest
     * @Feature({"Sync"})
     */
    @FlakyTest(message = "https://crbug.com/616456")
    public void testSignInWithOpenSettings() throws Exception {
        final Account testAccount = SigninTestUtil.addTestAccount();
        final Preferences prefActivity = processFirstRun(testAccount.name, ShowSettings.YES);

        // User should be signed in and the sync backend should initialize, but sync should not
        // become fully active until the settings page is closed.
        assertEquals(testAccount, SigninTestUtil.getCurrentAccount());
        SyncTestUtil.waitForEngineInitialized();
        assertFalse(SyncTestUtil.isSyncActive());

        // Close the settings fragment.
        AccountManagementFragment fragment =
                (AccountManagementFragment) prefActivity.getFragmentForTest();
        assertNotNull(fragment);
        prefActivity.getFragmentManager().beginTransaction().remove(fragment).commit();

        // Sync should immediately become active.
        assertTrue(SyncTestUtil.isSyncActive());
    }

    // Test that not signing in through FirstRun does not sign in sync.
    @SmallTest
    @Feature({"Sync"})
    public void testNoSignIn() throws Exception {
        SigninTestUtil.addTestAccount();
        assertFalse(SyncTestUtil.isSyncRequested());
        processFirstRun(null, ShowSettings.NO);
        assertNull(SigninTestUtil.getCurrentAccount());
        assertFalse(SyncTestUtil.isSyncRequested());
    }

    /**
     * Execute the FirstRun code using the given parameters.
     *
     * @param account The account name to sign in, or null.
     * @param showSettings Whether to show the settings page.
     * @return The Preferences activity if showSettings was YES; null otherwise.
     */
    private Preferences processFirstRun(String account, ShowSettings showSettings) {
        FirstRunSignInProcessor.setFirstRunFlowSignInComplete(mContext, false);
        Bundle data = new Bundle();
        data.putString(FirstRunActivity.RESULT_SIGNIN_ACCOUNT_NAME, account);
        data.putBoolean(
                FirstRunActivity.RESULT_SHOW_SIGNIN_SETTINGS, showSettings == ShowSettings.YES);
        FirstRunSignInProcessor.finalizeFirstRunFlowState(mContext, data);

        Preferences prefActivity = null;
        if (showSettings == ShowSettings.YES) {
            prefActivity = ActivityUtils.waitForActivity(
                    getInstrumentation(), Preferences.class, new Runnable() {
                        @Override
                        public void run() {
                            processFirstRunOnUiThread();
                        }
                    });
            assertNotNull("Could not find the preferences activity", prefActivity);
        } else {
            processFirstRunOnUiThread();
        }

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return FirstRunSignInProcessor.getFirstRunFlowSignInComplete(mContext);
            }
        });
        return prefActivity;
    }

    private void processFirstRunOnUiThread() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FirstRunSignInProcessor.start(getActivity());
            }
        });
    }
}
