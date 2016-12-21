// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.support.test.filters.MediumTest;
import android.support.v7.app.AlertDialog;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.net.test.EmbeddedTestServer;

/** Test suite for Web Share (navigator.share) functionality. */
public class WebShareTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String TEST_FILE = "/content/test/data/android/webshare.html";

    private EmbeddedTestServer mTestServer;

    private String mUrl;

    private Tab mTab;
    private WebShareUpdateWaiter mUpdateWaiter;

    private Intent mReceivedIntent;

    /** Waits until the JavaScript code supplies a result. */
    private class WebShareUpdateWaiter extends EmptyTabObserver {
        private CallbackHelper mCallbackHelper;
        private String mStatus;

        public WebShareUpdateWaiter() {
            mCallbackHelper = new CallbackHelper();
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            String title = getActivity().getActivityTab().getTitle();
            // Wait until the title indicates either success or failure.
            if (!title.equals("Success") && !title.startsWith("Fail:")) return;
            mStatus = title;
            mCallbackHelper.notifyCalled();
        }

        public String waitForUpdate() throws Exception {
            mCallbackHelper.waitForCallback(0);
            return mStatus;
        }
    }

    public WebShareTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());

        mUrl = mTestServer.getURL(TEST_FILE);

        mTab = getActivity().getActivityTab();
        mUpdateWaiter = new WebShareUpdateWaiter();
        mTab.addObserver(mUpdateWaiter);

        mReceivedIntent = null;
    }

    @Override
    protected void tearDown() throws Exception {
        mTab.removeObserver(mUpdateWaiter);
        mTestServer.stopAndDestroyServer();

        // Clean up some state that might have been changed by tests.
        ShareHelper.setForceCustomChooserForTesting(false);
        ShareHelper.setFakeIntentReceiverForTesting(null);

        super.tearDown();
    }

    /**
     * Verify that WebShare is missing by default (without a flag).
     * @throws Exception
     */
    @MediumTest
    @Feature({"WebShare"})
    public void testWebShareMissingWithoutFlag() throws Exception {
        loadUrl(mUrl);
        runJavaScriptCodeInCurrentTab("initiate_share()");
        assertEquals("Fail: navigator.share === undefined", mUpdateWaiter.waitForUpdate());
    }

    /**
     * Verify that WebShare fails if called without a user gesture.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-blink-features=WebShare")
    @Feature({"WebShare"})
    public void testWebShareNoUserGesture() throws Exception {
        loadUrl(mUrl);
        runJavaScriptCodeInCurrentTab("initiate_share()");
        assertEquals(
                "Fail: SecurityError: Must be handling a user gesture to perform a share request.",
                mUpdateWaiter.waitForUpdate());
    }

    /**
     * Verify that WebShare fails if the origin trial is disabled.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add({
            "enable-blink-features=WebShare", "origin-trial-disabled-features=WebShare"})
    @Feature({"WebShare"})
    public void testWebShareOriginTrialDisabled() throws Exception {
        loadUrl(mUrl);
        singleClickView(mTab.getView());
        assertEquals("Fail: SecurityError: WebShare is disabled.", mUpdateWaiter.waitForUpdate());
    }

    /**
     * Verify WebShare fails if share is called from a user gesture, and canceled.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-blink-features=WebShare")
    @Feature({"WebShare"})
    public void testWebShareCancel() throws Exception {
        // This test tests functionality that is only available post Lollipop MR1.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP_MR1) return;

        // Set up ShareHelper to ignore the intent (without showing a picker). This simulates the
        // user canceling the dialog.
        ShareHelper.setFakeIntentReceiverForTesting(new ShareHelper.FakeIntentReceiver() {
            public void setIntentToSendBack(Intent intent) {}

            public void onCustomChooserShown(AlertDialog dialog) {}

            public void fireIntent(Context context, Intent intent) {
                // Click again to start another share. This is necessary to work around
                // https://crbug.com/636274 (callback is not canceled until next share is
                // initiated). This also serves as a regression test for https://crbug.com/640324.
                singleClickView(mTab.getView());
            }
        });

        loadUrl(mUrl);
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        singleClickView(mTab.getView());
        assertEquals("Fail: AbortError: Share canceled", mUpdateWaiter.waitForUpdate());
    }

    /**
     * Verify WebShare succeeds if share is called from a user gesture, and app chosen.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-blink-features=WebShare")
    @Feature({"WebShare"})
    public void testWebShareSuccess() throws Exception {
        // This test tests functionality that is only available post Lollipop MR1.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP_MR1) return;

        // Set up ShareHelper to immediately succeed (without showing a picker).
        ShareHelper.setFakeIntentReceiverForTesting(new ShareHelper.FakeIntentReceiver() {
            private Intent mIntentToSendBack;

            public void setIntentToSendBack(Intent intent) {
                mIntentToSendBack = intent;
            }

            public void onCustomChooserShown(AlertDialog dialog) {}

            public void fireIntent(Context context, Intent intent) {
                mReceivedIntent = intent;

                if (context == null) return;

                // Send the intent back, which indicates that the user made a choice. (Normally,
                // this would have EXTRA_CHOSEN_COMPONENT set, but for the test, we do not set any
                // chosen target app.)
                context.sendBroadcast(mIntentToSendBack);
            }
        });

        loadUrl(mUrl);
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        singleClickView(mTab.getView());
        assertEquals("Success", mUpdateWaiter.waitForUpdate());

        // The actual intent to be delivered to the target is in the EXTRA_INTENT of the chooser
        // intent.
        assertNotNull(mReceivedIntent);
        assertTrue(mReceivedIntent.hasExtra(Intent.EXTRA_INTENT));
        Intent innerIntent = mReceivedIntent.getParcelableExtra(Intent.EXTRA_INTENT);
        assertNotNull(innerIntent);
        assertEquals(Intent.ACTION_SEND, innerIntent.getAction());
        assertTrue(innerIntent.hasExtra(Intent.EXTRA_SUBJECT));
        assertEquals("Test Title", innerIntent.getStringExtra(Intent.EXTRA_SUBJECT));
        assertTrue(innerIntent.hasExtra(Intent.EXTRA_TEXT));
        assertEquals("Test Text https://test.url/", innerIntent.getStringExtra(Intent.EXTRA_TEXT));
    }

    /**
     * Verify WebShare fails if share is called from a user gesture, and canceled.
     *
     * Simulates pre-Lollipop-LMR1 system (different intent picker).
     *
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-blink-features=WebShare")
    @Feature({"WebShare"})
    public void testWebShareCancelPreLMR1() throws Exception {
        ShareHelper.setFakeIntentReceiverForTesting(new ShareHelper.FakeIntentReceiver() {
            public void setIntentToSendBack(Intent intent) {}

            public void onCustomChooserShown(AlertDialog dialog) {
                // Cancel the chooser dialog.
                dialog.dismiss();
            }

            public void fireIntent(Context context, Intent intent) {}
        });

        ShareHelper.setForceCustomChooserForTesting(true);

        loadUrl(mUrl);
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        singleClickView(mTab.getView());
        assertEquals("Fail: AbortError: Share canceled", mUpdateWaiter.waitForUpdate());
    }

    /**
     * Verify WebShare succeeds if share is called from a user gesture, and app chosen.
     *
     * Simulates pre-Lollipop-LMR1 system (different intent picker).
     *
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-blink-features=WebShare")
    @Feature({"WebShare"})
    public void testWebShareSuccessPreLMR1() throws Exception {
        ShareHelper.setFakeIntentReceiverForTesting(new ShareHelper.FakeIntentReceiver() {
            public void setIntentToSendBack(Intent intent) {}

            public void onCustomChooserShown(AlertDialog dialog) {
                // Click on an app (it doesn't matter which, because we will hook the intent).
                assert dialog.getListView().getCount() > 0;
                dialog
                    .getListView()
                    .performItemClick(null, 0, dialog.getListView().getItemIdAtPosition(0));
            }

            public void fireIntent(Context context, Intent intent) {
                mReceivedIntent = intent;
            }
        });

        ShareHelper.setForceCustomChooserForTesting(true);

        loadUrl(mUrl);
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        singleClickView(mTab.getView());
        assertEquals("Success", mUpdateWaiter.waitForUpdate());

        assertNotNull(mReceivedIntent);
        assertEquals(Intent.ACTION_SEND, mReceivedIntent.getAction());
        assertTrue(mReceivedIntent.hasExtra(Intent.EXTRA_SUBJECT));
        assertEquals("Test Title", mReceivedIntent.getStringExtra(Intent.EXTRA_SUBJECT));
        assertTrue(mReceivedIntent.hasExtra(Intent.EXTRA_TEXT));
        assertEquals("Test Text https://test.url/",
                     mReceivedIntent.getStringExtra(Intent.EXTRA_TEXT));
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
