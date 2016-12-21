// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.app.Instrumentation.ActivityMonitor;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.sync.FakeProfileSyncService;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.test.util.ApplicationData;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.content.browser.test.NativeLibraryTestBase;

/**
 * Tests for PassphraseActivity.
 */
public class PassphraseActivityTest extends NativeLibraryTestBase {
    private static final String TEST_ACCOUNT = "test@gmail.com";

    private Context mContext;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getTargetContext();
        SigninTestUtil.setUpAuthForTest(getInstrumentation());
        ApplicationData.clearAppData(mContext);
        loadNativeLibraryAndInitBrowserProcess();
    }

    @Override
    protected void tearDown() throws Exception {
        // Clear ProfileSyncService in case it was mocked.
        ProfileSyncService.overrideForTests(null);
        super.tearDown();
        SigninTestUtil.resetSigninState();
    }

    /**
     * This is a regression test for http://crbug.com/469890.
     */
    @SmallTest
    @Feature({"Sync"})
    @RetryOnFailure
    public void testCallbackAfterBackgrounded() throws Exception {
        getInstrumentation().waitForIdleSync();
        SigninTestUtil.addAndSignInTestAccount();

        // Override before creating the activity so we know initialized is false.
        overrideProfileSyncService();

        // PassphraseActivity won't start if an account isn't set.
        assertNotNull(ChromeSigninController.get(mContext).getSignedInAccountName());

        // Create the activity.
        final PassphraseActivity activity = launchPassphraseActivity();
        assertNotNull(activity);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Fake backgrounding the activity.
                Bundle bundle = new Bundle();
                getInstrumentation().callActivityOnPause(activity);
                getInstrumentation().callActivityOnSaveInstanceState(activity, bundle);
                // Fake sync's backend finishing its initialization.
                FakeProfileSyncService pss = (FakeProfileSyncService) ProfileSyncService.get();
                pss.setEngineInitialized(true);
                pss.syncStateChanged();
            }
        });
        // Nothing crashed; success!
    }

    private PassphraseActivity launchPassphraseActivity() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setComponent(new ComponentName(mContext, PassphraseActivity.class));
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        // This activity will become the start of a new task on this history stack.
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // Clears the task stack above this activity if it already exists.
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        ActivityMonitor monitor =
                getInstrumentation().addMonitor(PassphraseActivity.class.getName(), null, false);
        mContext.startActivity(intent);
        return (PassphraseActivity) getInstrumentation().waitForMonitor(monitor);
    }

    private void overrideProfileSyncService() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // PSS has to be constructed on the UI thread.
                ProfileSyncService.overrideForTests(new FakeProfileSyncService());
            }
        });
    }
}
