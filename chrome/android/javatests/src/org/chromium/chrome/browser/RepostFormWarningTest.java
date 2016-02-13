// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Environment;
import android.support.v7.app.AlertDialog;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests verifying that form resubmission dialogs are correctly displayed and handled.
 */
public class RepostFormWarningTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    // Active tab.
    private Tab mTab;
    // Callback helper that manages waiting for pageloads to finish.
    private TestCallbackHelperContainer mCallbackHelper;

    private EmbeddedTestServer mTestServer;

    public RepostFormWarningTest() {
        super(ChromeActivity.class);
        mSkipCheckHttpServer = true;
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mTab = getActivity().getActivityTab();
        mCallbackHelper = new TestCallbackHelperContainer(mTab.getContentViewCore());
        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
    }

    @Override
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /** Verifies that the form resubmission warning is not displayed upon first POST navigation. */
    @MediumTest
    @Feature({"Navigation"})
    public void testFormFirstNavigation() throws Throwable {
        // Load the url posting data for the first time.
        postNavigation();
        mCallbackHelper.getOnPageFinishedHelper().waitForCallback(0);
        getInstrumentation().waitForIdleSync();

        // Verify that the form resubmission warning was not shown.
        assertNull("Form resubmission warning shown upon first load.",
                RepostFormWarningDialog.getCurrentDialog());
    }

    /** Verifies that confirming the form reload performs the reload. */
    @MediumTest
    @Feature({"Navigation"})
    public void testFormResubmissionContinue() throws Throwable {
        // Load the url posting data for the first time.
        postNavigation();
        mCallbackHelper.getOnPageFinishedHelper().waitForCallback(0);

        // Trigger a reload and wait for the warning to be displayed.
        reload();
        AlertDialog dialog = waitForRepostFormWarningDialog();

        // Click "Continue" and verify that the page is reloaded.
        clickButton(dialog, AlertDialog.BUTTON_POSITIVE);
        mCallbackHelper.getOnPageFinishedHelper().waitForCallback(1);

        // Verify that the reference to the dialog in RepostFormWarningDialog was cleared.
        assertNull("Form resubmission warning dialog was not dismissed correctly.",
                RepostFormWarningDialog.getCurrentDialog());
    }

    /**
     * Verifies that cancelling the form reload prevents it from happening. Currently the test waits
     * after the "Cancel" button is clicked to verify that the load was not triggered, which blocks
     * for CallbackHelper's default timeout upon each execution.
     */
    @SmallTest
    @Feature({"Navigation"})
    public void testFormResubmissionCancel() throws Throwable {
        // Load the url posting data for the first time.
        postNavigation();
        mCallbackHelper.getOnPageFinishedHelper().waitForCallback(0);

        // Trigger a reload and wait for the warning to be displayed.
        reload();
        AlertDialog dialog = waitForRepostFormWarningDialog();

        // Click "Cancel" and verify that the page is not reloaded.
        clickButton(dialog, AlertDialog.BUTTON_NEGATIVE);
        boolean timedOut = false;
        try {
            mCallbackHelper.getOnPageFinishedHelper().waitForCallback(1);
        } catch (TimeoutException ex) {
            timedOut = true;
        }
        assertTrue("Page was reloaded despite selecting Cancel.", timedOut);

        // Verify that the reference to the dialog in RepostFormWarningDialog was cleared.
        assertNull("Form resubmission warning dialog was not dismissed correctly.",
                RepostFormWarningDialog.getCurrentDialog());
    }

    private AlertDialog waitForRepostFormWarningDialog() throws InterruptedException {
        CriteriaHelper.pollForUIThreadCriteria(
                new Criteria("Form resubmission warning not shown") {
                    @Override
                    public boolean isSatisfied() {
                        return RepostFormWarningDialog.getCurrentDialog() != null;
                    }
                });
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<AlertDialog>() {
            @Override
            public AlertDialog call() throws Exception {
                return (AlertDialog) RepostFormWarningDialog.getCurrentDialog();
            }
        });
    }

    /** Performs a POST navigation in mTab. */
    private void postNavigation() throws Throwable {
        final String url = "/chrome/test/data/empty.html";
        final byte[] postData = new byte[] { 42 };

        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                mTab.loadUrl(LoadUrlParams.createLoadHttpPostParams(
                        mTestServer.getURL(url), postData));
            }
        });
    }

    /** Reloads mTab. */
    private void reload() throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                mTab.reload();
            }
        });
    }

    /** Clicks the given button in the given dialog. */
    private void clickButton(final AlertDialog dialog, final int buttonId) throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                dialog.getButton(buttonId).performClick();
            }
        });
    }
}
