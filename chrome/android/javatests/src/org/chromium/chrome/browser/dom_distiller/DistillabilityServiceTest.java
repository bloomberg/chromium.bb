// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.compositor.bottombar.readermode.ReaderModePanel;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content.browser.test.util.TestWebContentsObserver;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 * Tests for making sure the distillability service is communicating correctly.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({"enable-dom-distiller", "reader-mode-heuristics=alwaystrue",
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class DistillabilityServiceTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_PAGE = "/chrome/test/data/android/simple.html";

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Make sure that Reader Mode appears after navigating from a native page.
     */
    @Test
    @Feature({"Distillability-Service"})
    @MediumTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @DisabledTest
    public void testServiceAliveAfterNativePage() throws InterruptedException, TimeoutException {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());

        final ReaderModePanel panel =
                mActivityTestRule.getActivity().getReaderModeManager().getPanelForTesting();

        TestWebContentsObserver observer = new TestWebContentsObserver(
                mActivityTestRule.getActivity().getActivityTab().getWebContents());
        OnPageFinishedHelper finishHelper = observer.getOnPageFinishedHelper();

        // Navigate to a native page.
        int curCallCount = finishHelper.getCallCount();
        mActivityTestRule.loadUrl("chrome://history");
        finishHelper.waitForCallback(curCallCount, 1);
        Assert.assertFalse(panel.isShowing());

        // Navigate to a normal page.
        curCallCount = finishHelper.getCallCount();
        mActivityTestRule.loadUrl(testServer.getURL(TEST_PAGE));
        finishHelper.waitForCallback(curCallCount, 1);
        Assert.assertTrue(panel.isShowing());
    }
}
