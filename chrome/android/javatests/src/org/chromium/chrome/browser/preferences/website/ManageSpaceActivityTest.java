// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.annotation.TargetApi;
import android.content.Intent;
import android.os.Build;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.v7.app.AlertDialog;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests for ManageSpaceActivity.
 */
@TargetApi(Build.VERSION_CODES.KITKAT)
@CommandLineFlags.Add({"enable-site-engagement"})
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT)
public class ManageSpaceActivityTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private EmbeddedTestServer mTestServer;

    public ManageSpaceActivityTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        if (getName().equals("testClearUnimporantWithoutChromeStart")) {
            return;
        }
        startMainActivityOnBlankPage();
    }

    private ManageSpaceActivity startManageSpaceActivity() {
        Intent intent =
                new Intent(getInstrumentation().getTargetContext(), ManageSpaceActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        return (ManageSpaceActivity) getInstrumentation().startActivitySync(intent);
    }

    public void waitForClearButtonEnabled(final ManageSpaceActivity activity) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return activity.getClearUnimportantButton().isEnabled();
            }
        });
    }

    public Runnable getClickClearRunnable(final ManageSpaceActivity activity) {
        return new Runnable() {
            @Override
            public void run() {
                activity.onClick(activity.getClearUnimportantButton());
            }
        };
    }

    public void waitForDialogShowing(final ManageSpaceActivity activity) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return activity.getUnimportantConfirmDialog().isShowing();
            }
        });
    }

    public Runnable getPressClearRunnable(final AlertDialog dialog) {
        return new Runnable() {
            @Override
            public void run() {
                dialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
            }
        };
    }

    @SmallTest
    @RetryOnFailure
    public void testLaunchActivity() {
        startManageSpaceActivity();
    }

    @MediumTest
    @RetryOnFailure
    @Feature({"SiteEngagement"})
    public void testClearUnimportantOnly() throws Exception {
        final String cookiesUrl =
                mTestServer.getURL("/chrome/test/data/android/storage_persistance.html");
        final String serverOrigin = mTestServer.getURL("/");

        loadUrl(cookiesUrl + "#clear");
        assertEquals("false", runJavaScriptCodeInCurrentTab("hasAllStorage()"));
        runJavaScriptCodeInCurrentTab("setStorage()");
        assertEquals("true", runJavaScriptCodeInCurrentTab("hasAllStorage()"));
        loadUrl("about:blank");

        // Now we set the origin as important, and check that we don't clear it.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge.markOriginAsImportantForTesting(serverOrigin);
            }
        });

        ManageSpaceActivity manageSpaceActivity = startManageSpaceActivity();
        // Click 'clear' in the CBD screen.
        waitForClearButtonEnabled(manageSpaceActivity);
        ThreadUtils.runOnUiThreadBlocking(getClickClearRunnable(manageSpaceActivity));
        // Press 'clear' in our dialog.
        waitForDialogShowing(manageSpaceActivity);
        ThreadUtils.runOnUiThreadBlocking(
                getPressClearRunnable(manageSpaceActivity.getUnimportantConfirmDialog()));
        waitForClearButtonEnabled(manageSpaceActivity);
        manageSpaceActivity.finish();

        loadUrl(cookiesUrl);
        assertEquals("true", runJavaScriptCodeInCurrentTab("hasAllStorage()"));
    }

    @MediumTest
    @Feature({"SiteEngagement"})
    public void testClearUnimporantWithoutChromeStart() throws Exception {
        ManageSpaceActivity manageSpaceActivity = startManageSpaceActivity();
        // Click 'clear' in the CBD screen.
        waitForClearButtonEnabled(manageSpaceActivity);
        ThreadUtils.runOnUiThreadBlocking(getClickClearRunnable(manageSpaceActivity));
        // Press 'clear' in our dialog.
        waitForDialogShowing(manageSpaceActivity);
        ThreadUtils.runOnUiThreadBlocking(
                getPressClearRunnable(manageSpaceActivity.getUnimportantConfirmDialog()));
        waitForClearButtonEnabled(manageSpaceActivity);
        manageSpaceActivity.finish();
    }

    // TODO(dmurph): Test the other buttons. One should go to the site storage list, and the other
    //               should reset all app data.
}
