// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.content.browser.test.util.TestWebContentsObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/**
 * Tests related to the ToolbarProgressBar.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class ToolbarProgressBarTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_PAGE = "/chrome/test/data/android/progressbar_test.html";

    private ToolbarProgressBar mProgressBar;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mProgressBar = mActivityTestRule.getActivity()
                               .getToolbarManager()
                               .getToolbarLayout()
                               .getProgressBar();
        ThreadUtils.runOnUiThreadBlocking(() -> mProgressBar.endProgressForTesting());
        mProgressBar.resetStartCountForTesting();
    }

    /**
     * Test that the progress bar only traverses the page a single time per navigation.
     */
    @Test
    @Feature({"Android-Toolbar"})
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testProgressBarTraversesScreenOnce() throws InterruptedException, TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        final WebContents webContents =
                mActivityTestRule.getActivity().getActivityTab().getWebContents();

        TestWebContentsObserver observer = new TestWebContentsObserver(webContents);
        // Start and stop load events are carefully tracked; there should be two start-stop pairs
        // that do not overlap.
        OnPageStartedHelper startHelper = observer.getOnPageStartedHelper();
        OnPageFinishedHelper finishHelper = observer.getOnPageFinishedHelper();

        // Ensure no load events have occurred yet.
        Assert.assertEquals(0, startHelper.getCallCount());
        Assert.assertEquals(0, finishHelper.getCallCount());

        mActivityTestRule.loadUrl(testServer.getURL(TEST_PAGE));

        // Wait for the initial page to be loaded if it hasn't already.
        if (finishHelper.getCallCount() == 0) {
            finishHelper.waitForCallback(finishHelper.getCallCount(), 1);
        }

        // Exactly one start load and one finish load event should have occurred.
        Assert.assertEquals(1, startHelper.getCallCount());
        Assert.assertEquals(1, finishHelper.getCallCount());

        // Load content in the iframe of the test page to trigger another load.
        JavaScriptUtils.executeJavaScript(webContents, "loadIframeInPage();");

        // A load start will be triggered.
        startHelper.waitForCallback(startHelper.getCallCount(), 1);
        Assert.assertEquals(2, startHelper.getCallCount());

        // Wait for the iframe to finish loading.
        finishHelper.waitForCallback(finishHelper.getCallCount(), 1);
        Assert.assertEquals(2, finishHelper.getCallCount());

        // Though the page triggered two load events, the progress bar should have only appeared a
        // single time.
        Assert.assertEquals(1, mProgressBar.getStartCountForTesting());
    }

    /**
     * Test that the progress bar only traverses the page a single time per navigation.
     */
    @Test
    @Feature({"Android-Toolbar"})
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testProgressBarRestartsAtZero() throws InterruptedException, TimeoutException {
        // Reset progress bar start count in case anything else triggered it.
        mProgressBar.resetStartCountForTesting();

        ThreadUtils.runOnUiThreadBlocking(() -> mProgressBar.start());

        Assert.assertEquals(0, mProgressBar.getProgress(), MathUtils.EPSILON);
        Assert.assertFalse(mProgressBar.isThrottledProgressAnimatorRunning());

        ThreadUtils.runOnUiThreadBlocking(() -> mProgressBar.setProgress(0.5f));

        Assert.assertFalse(mProgressBar.getProgress() <= 0);
        Assert.assertTrue(mProgressBar.isThrottledProgressAnimatorRunning());

        ThreadUtils.runOnUiThreadBlocking(() -> mProgressBar.setProgress(0.5f));
        ThreadUtils.runOnUiThreadBlocking(() -> mProgressBar.finish(false));

        // Restart progress.
        ThreadUtils.runOnUiThreadBlocking(() -> mProgressBar.start());

        Assert.assertEquals(0, mProgressBar.getProgress(), MathUtils.EPSILON);
        Assert.assertFalse(mProgressBar.isThrottledProgressAnimatorRunning());
    }
}
