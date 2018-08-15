// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.NATIVE_URLS_OF_INTEREST;
import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI.PaymentRequestObserverForTest;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.util.VrBrowserTransitionUtils;
import org.chromium.chrome.browser.vr.util.VrShellDelegateUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * End-to-end tests for native UI presentation in VR Browser mode.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-webvr"})
@Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
public class VrBrowserNativeUiTest {
    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mVrTestRule = new ChromeTabbedActivityVrTestRule();

    private VrBrowserTestFramework mVrBrowserTestFramework;

    private static final String TEST_PAGE_2D_URL =
            VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_2d_page");

    @Before
    public void setUp() throws Exception {
        mVrBrowserTestFramework = new VrBrowserTestFramework(mVrTestRule);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
    }

    /**
     * Tests that URLs are not shown for native UI.
     */
    @Test
    @MediumTest
    public void testUrlOnNativeUi()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        for (String url : NATIVE_URLS_OF_INTEREST) {
            mVrTestRule.loadUrl(url, PAGE_LOAD_TIMEOUT_S);
            Assert.assertFalse("URL is being shown for native page " + url,
                    TestVrShellDelegate.isDisplayingUrlForTesting());
        }
    }

    /**
     * Tests that URLs are shown for non-native UI.
     */
    @Test
    @MediumTest
    public void testUrlOnNonNativeUi()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        mVrTestRule.loadUrl(TEST_PAGE_2D_URL, PAGE_LOAD_TIMEOUT_S);
        Assert.assertTrue("URL is not being show for non-native page",
                TestVrShellDelegate.isDisplayingUrlForTesting());
    }

    /**
     * Tests that the Payment Request API is supressed in the VR Browser and its native UI does not
     * show. Automation of a manual test from https://crbug.com/862162.
     */
    @Test
    @MediumTest
    public void testPaymentRequest() throws InterruptedException {
        // We can't request payment on file:// URLs, so use a local server.
        EmbeddedTestServer server =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                server.getURL(VrBrowserTestFramework.getEmbeddedServerPathForHtmlTestFile(
                        "test_payment_request")),
                PAGE_LOAD_TIMEOUT_S);
        // Set up an observer so we'll know if the payment request is shown.
        AtomicBoolean requestShown = new AtomicBoolean(false);
        PaymentRequestObserverForTest observer = new PaymentRequestObserverForTest() {
            @Override
            public void onPaymentRequestReadyForInput(PaymentRequestUI ui) {
                requestShown.set(true);
            }
            @Override
            public void onPaymentRequestReadyToPay(PaymentRequestUI ui) {}
            @Override
            public void onPaymentRequestSelectionChecked(PaymentRequestUI ui) {}
            @Override
            public void onPaymentRequestResultReady(PaymentRequestUI ui) {}
        };
        PaymentRequestUI.setPaymentRequestObserverForTest(observer);
        // Request payment and wait for the promise to auto-reject.
        mVrBrowserTestFramework.executeStepAndWait("stepRequestPayment()");
        // Ensure that the native UI wasn't shown even though the request was rejected.
        // Need to sleep for a bit in order to allow the UI to show if it's going to.
        SystemClock.sleep(1000);
        Assert.assertFalse("Native Payment Request UI was shown", requestShown.get());
        // Ensure we weren't somehow kicked out of VR from this.
        Assert.assertTrue("Payment request caused VR exit",
                VrShellDelegateUtils.getDelegateInstance().isVrEntryComplete());
        mVrBrowserTestFramework.endTest();
        server.stopAndDestroyServer();
    }
}
