// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for service worker based payment apps.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
public class PaymentRequestServiceWorkerPaymentAppTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_bobpay_test.html");

    /**
     * Installs a mock service worker based payment app for testing.
     *
     * @param hasSupportedMethods Whether the mock payment app has supported payment methods.
     */
    private void installMockServiceWorkerPaymentApp(final boolean hasSupportedMethods) {
        PaymentAppFactory.getInstance().addAdditionalFactory(
                (webContents, methodNames, callback) -> {
                    ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);
                    String[] supportedMethodNames = {"https://bobpay.com", "basic-card"};
                    callback.onPaymentAppCreated(
                            new ServiceWorkerPaymentApp(webContents, 0 /* registrationId */,
                                    UriUtils.parseUriFromString("https://bobpay.com") /* scope */,
                                    "BobPay" /* label */, null /* sublabel*/,
                                    "https://bobpay.com" /* tertiarylabel */, null /* icon */,
                                    hasSupportedMethods ? supportedMethodNames
                                                        : new String[0] /* methodNames */,
                                    new String[0] /* preferredRelatedApplicationIds */));
                    callback.onAllPaymentAppsCreated();
                });
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoSupportedPaymentMethods()
            throws InterruptedException, ExecutionException, TimeoutException {
        installMockServiceWorkerPaymentApp(false);
        mPaymentRequestTestRule.openPageAndClickBuyAndWait(mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"show() rejected", "The payment method is not supported"});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testHasSupportedPaymentMethods()
            throws InterruptedException, ExecutionException, TimeoutException {
        installMockServiceWorkerPaymentApp(true);
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        // TODO(tommyt): crbug.com/669876. Expand this test as we implement more
        // service worker based payment app functionality.
    }
}
