// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.os.Bundle;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.firstrun.FirstRunActivity;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.signin.ChromeSigninController;

/**
 * Tests for the first run experience.
 */
@CommandLineFlags.Remove(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class FirstRunTest extends SyncTestBase {
    private static final String TAG = "FirstRunTest";

    private enum ShowSyncSettings {
        YES,
        NO;
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityFromLauncher();
    }

    // Test that signing in through FirstRun signs in and starts sync.
    @SmallTest
    @Feature({"Sync"})
    public void testFreSignin() throws Exception {
        Account testAccount = SigninTestUtil.get().addTestAccount();
        SyncTestUtil.verifySyncIsSignedOut(mContext);
        assertFalse(AndroidSyncSettings.isChromeSyncEnabled(mContext));
        processFirstRun(testAccount.name, ShowSyncSettings.NO);
        assertEquals(
                testAccount.name, ChromeSigninController.get(mContext).getSignedInAccountName());
        SyncTestUtil.verifySyncIsActiveForAccount(mContext, testAccount);
    }

    // Test that not signing in through FirstRun does not sign in sync.
    @SmallTest
    @Feature({"Sync"})
    public void testFreNoSignin() throws Exception {
        SigninTestUtil.get().addTestAccount();
        SyncTestUtil.verifySyncIsSignedOut(mContext);
        assertFalse(AndroidSyncSettings.isChromeSyncEnabled(mContext));
        processFirstRun(null, ShowSyncSettings.NO);
        assertNull(ChromeSigninController.get(mContext).getSignedInAccountName());
        SyncTestUtil.verifySyncIsSignedOut(mContext);
        assertFalse(AndroidSyncSettings.isChromeSyncEnabled(mContext));
    }

    /**
     * Execute the FirstRun code using the given parameters.
     *
     * @param account The account name to sign in, or null.
     * @param showSyncSettings Whether to show the sync settings page.
     */
    private void processFirstRun(String account, ShowSyncSettings showSyncSettings)
            throws InterruptedException {
        FirstRunSignInProcessor.setFirstRunFlowSignInComplete(mContext, false);
        Bundle data = new Bundle();
        data.putString(FirstRunActivity.RESULT_SIGNIN_ACCOUNT_NAME, account);
        data.putBoolean(FirstRunActivity.RESULT_SHOW_SIGNIN_SETTINGS,
                showSyncSettings == ShowSyncSettings.YES);
        FirstRunSignInProcessor.finalizeFirstRunFlowState(mContext, data);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FirstRunSignInProcessor.start(getActivity());
            }
        });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return FirstRunSignInProcessor.getFirstRunFlowSignInComplete(mContext);
            }
        });
    }
}
