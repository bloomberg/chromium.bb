// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.View;

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
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.content.browser.test.util.TestWebContentsObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

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

    static final int TEST_WAIT_TIME_MS = 60000;
    private static final String TEST_PAGE = "/chrome/test/data/android/progressbar_test.html";

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Test that the progress bar only traverses the page a single time per navigation.
     */
    @Test
    @Feature({"Android-Toolbar"})
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testToolbarTraversesScreenOnce() throws InterruptedException, TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        final WebContents webContents =
                mActivityTestRule.getActivity().getActivityTab().getWebContents();

        TestWebContentsObserver observer = new TestWebContentsObserver(webContents);
        // Start and stop load events are carefully tracked; there should be two start-stop pairs
        // that do not overlap.
        OnPageStartedHelper startHelper = observer.getOnPageStartedHelper();
        OnPageFinishedHelper finishHelper = observer.getOnPageFinishedHelper();

        ToolbarProgressBar progressBar = mActivityTestRule.getActivity()
                                                 .getToolbarManager()
                                                 .getToolbarLayout()
                                                 .getProgressBar();

        // Reset progress bar start count in case anything else triggered it.
        progressBar.resetStartCountForTesting();

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
        Assert.assertEquals(1, progressBar.getStartCountForTesting());
    }

    /**
     * Test that calling progressBar.setProgress(# > 0) followed by progressBar.setProgress(0)
     * results in a hidden progress bar.
     * @throws InterruptedException
     */
    @Test
    @Feature({"Android-Toolbar"})
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testProgressBarDisappearsAfterFastShowHide() throws InterruptedException {
        // onAnimationEnd will be signaled on progress bar showing/hiding animation end.
        final Object onAnimationEnd = new Object();
        final AtomicBoolean animationEnded = new AtomicBoolean(false);
        final AtomicReference<ToolbarProgressBar> progressBar =
                new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                progressBar.set(mActivityTestRule.getActivity()
                                        .getToolbarManager()
                                        .getToolbarLayout()
                                        .getProgressBar());
                progressBar.get().setAlphaAnimationDuration(10);
                progressBar.get().setHidingDelay(10);
                progressBar.get().animate().setListener(new AnimatorListener() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                    }

                    @Override
                    public void onAnimationRepeat(Animator animation) {
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        synchronized (onAnimationEnd) {
                            animationEnded.set(true);
                            onAnimationEnd.notify();
                        }
                    }

                    @Override
                    public void onAnimationCancel(Animator animation) {
                    }
                });
            }
        });

        CriteriaHelper.pollUiThread(new Criteria("Progress bar not hidden at start") {
            @Override
            public boolean isSatisfied() {
                return progressBar.get().getVisibility() == View.INVISIBLE;
            }
        });

        // Make some progress and check that the progress bar is fully visible.
        animationEnded.set(false);
        synchronized (onAnimationEnd) {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    progressBar.get().start();
                    progressBar.get().setProgress(0.5f);
                }
            });

            long deadline = System.currentTimeMillis() + TEST_WAIT_TIME_MS;
            while (!animationEnded.get() && System.currentTimeMillis() < deadline) {
                onAnimationEnd.wait(deadline - System.currentTimeMillis());
            }
            Assert.assertEquals(1.0f, progressBar.get().getAlpha(), 0);
            Assert.assertEquals(View.VISIBLE, progressBar.get().getVisibility());
        }

        // Clear progress and check that the progress bar is hidden.
        animationEnded.set(false);
        synchronized (onAnimationEnd) {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    progressBar.get().finish(true);
                }
            });

            long deadline = System.currentTimeMillis() + TEST_WAIT_TIME_MS;
            while (!animationEnded.get() && System.currentTimeMillis() < deadline) {
                onAnimationEnd.wait(deadline - System.currentTimeMillis());
            }
            Assert.assertEquals(0.0f, progressBar.get().getAlpha(), 0);
            Assert.assertNotSame(View.VISIBLE, progressBar.get().getVisibility());
        }
    }
}
