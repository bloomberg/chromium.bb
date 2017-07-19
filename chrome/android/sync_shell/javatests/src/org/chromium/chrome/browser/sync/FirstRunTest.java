// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.firstrun.FirstRunActivity;
import org.chromium.chrome.browser.firstrun.FirstRunActivity.FirstRunActivityObserver;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.signin.AccountManagementFragment;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the first run experience.
 */
@CommandLineFlags.Remove(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@RetryOnFailure  // crbug.com/637448
public class FirstRunTest extends SyncTestBase {
    private static final String TEST_ACTION = "com.artificial.package.TEST_ACTION";

    private static enum ShowSettings {
        YES,
        NO;
    }

    private static final class TestObserver implements FirstRunActivityObserver {
        public final CallbackHelper flowIsKnownCallback = new CallbackHelper();

        @Override
        public void onFlowIsKnown(Bundle freProperties) {
            flowIsKnownCallback.notifyCalled();
        }

        @Override
        public void onAcceptTermsOfService() {}

        @Override
        public void onJumpToPage(int position) {}

        @Override
        public void onUpdateCachedEngineName() {}

        @Override
        public void onAbortFirstRunExperience() {}
    }

    private final TestObserver mTestObserver = new TestObserver();
    private FirstRunActivity mActivity;

    @Override
    public void startMainActivity() throws InterruptedException {
        FirstRunActivity.setObserverForTest(mTestObserver);

        // Starts up and waits for the FirstRunActivity to be ready.
        // This isn't exactly what startMainActivity is supposed to be doing, but short of a
        // refactoring of SyncTestBase to use something other than ChromeTabbedActivity, it's the
        // only way to reuse the rest of the setup and initialization code inside of it.
        final Instrumentation instrumentation = getInstrumentation();
        final Context context = instrumentation.getTargetContext();

        // Create an Intent that causes Chrome to run.
        final Intent intent = new Intent(TEST_ACTION);
        intent.setPackage(context.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // Start the FRE.
        final ActivityMonitor freMonitor =
                new ActivityMonitor(FirstRunActivity.class.getName(), null, false);
        instrumentation.addMonitor(freMonitor);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FirstRunFlowSequencer.launch(context, intent, false /* requiresBroadcast */,
                        false /* preferLightweightFre */);
            }
        });

        // Wait for the FRE to be ready to use.
        Activity activity =
                freMonitor.waitForActivityWithTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        instrumentation.removeMonitor(freMonitor);

        assertTrue(activity instanceof FirstRunActivity);
        mActivity = (FirstRunActivity) activity;

        try {
            mTestObserver.flowIsKnownCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            fail();
        }

        getInstrumentation().waitForIdleSync();
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
    }

    @Override
    public void tearDown() throws Exception {
        if (mActivity != null) mActivity.finish();
        super.tearDown();
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
                FirstRunSignInProcessor.start(mActivity);
            }
        });
    }
}
