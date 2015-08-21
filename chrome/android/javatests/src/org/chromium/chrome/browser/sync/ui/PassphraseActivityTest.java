// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.test.FlakyTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.sync.FakeProfileSyncService;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.test.util.ApplicationData;
import org.chromium.content.browser.test.NativeLibraryTestBase;
import org.chromium.sync.signin.ChromeSigninController;

/**
 * Tests for PassphraseActivity.
 */
public class PassphraseActivityTest extends NativeLibraryTestBase {
    private static final String TEST_ACCOUNT = "test@gmail.com";

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        ApplicationData.clearAppData(getInstrumentation().getTargetContext());
        loadNativeLibraryAndInitBrowserProcess();
    }

    @Override
    protected void tearDown() throws Exception {
        // Clear ProfileSyncService in case it was mocked.
        ProfileSyncService.overrideForTests(null);
        super.tearDown();
    }

    /**
     * This is a regression test for http://crbug.com/469890.
     * @SmallTest
     * Constantly fails on M, fine on other platforms: http://crbug.com/517590
     */
    @FlakyTest
    @Feature({"Sync"})
    public void testCallbackAfterBackgrounded() throws Exception {
        final Context context = getInstrumentation().getTargetContext();
        // Override before creating the activity so we know initialized is false.
        overrideProfileSyncService(context);
        // PassphraseActivity won't start if an account isn't set.
        ChromeSigninController.get(context).setSignedInAccountName(TEST_ACCOUNT);
        // Create the activity.
        final PassphraseActivity activity = launchPassphraseActivity(context);
        assertNotNull(activity);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Fake backgrounding the activity.
                Bundle bundle = new Bundle();
                getInstrumentation().callActivityOnPause(activity);
                getInstrumentation().callActivityOnSaveInstanceState(activity, bundle);
                // Fake sync's backend finishing its initialization.
                FakeProfileSyncService pss =
                        (FakeProfileSyncService) ProfileSyncService.get(context);
                pss.setSyncInitialized(true);
                pss.syncStateChanged();
            }
        });
        // Nothing crashed; success!
    }

    private PassphraseActivity launchPassphraseActivity(Context context) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setComponent(new ComponentName(context, PassphraseActivity.class));
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        // This activity will become the start of a new task on this history stack.
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // Clears the task stack above this activity if it already exists.
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        return launchActivityWithIntent(context.getPackageName(), PassphraseActivity.class, intent);
    }

    private void overrideProfileSyncService(final Context context) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // PSS has to be constructed on the UI thread.
                ProfileSyncService.overrideForTests(new FakeProfileSyncService(context));
            }
        });
    }
}
