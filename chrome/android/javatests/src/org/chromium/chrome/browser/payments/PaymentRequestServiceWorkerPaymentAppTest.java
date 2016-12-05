// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for service worker based payment apps.
 */
public class PaymentRequestServiceWorkerPaymentAppTest extends PaymentRequestTestBase {
    /** Flag for installing a service worker payment app without any payment options. */
    private static final int NO_OPTIONS = 0;

    /** Flag for installing a service worker payment app with one payment option. */
    private static final int ONE_OPTION = 1;

    /** Flag for installing a service worker payment app with two options. */
    private static final int TWO_OPTIONS = 2;

    public PaymentRequestServiceWorkerPaymentAppTest() {
        super("payment_request_bobpay_test.html");
    }

    /**
     * Installs a service worker based payment app for testing.
     *
     * @param optionPresence Whether the manifest has any payment options. Either NO_OPTIONS
     *                       ONE_OPTION or TWO_OPTIONS
     */
    private void installServiceWorkerPaymentApp(final int instrumentPresence) {
        PaymentAppFactory.setServiceWorkerPaymentAppBridgeForTest(
                new ServiceWorkerPaymentAppBridge() {
                    @Override
                    public List<Manifest> getAllAppManifests() {
                        ServiceWorkerPaymentAppBridge.Manifest testManifest =
                                new ServiceWorkerPaymentAppBridge.Manifest();
                        testManifest.scopeUrl = "https://bobpay.com/app";
                        testManifest.label = "BobPay";

                        if (instrumentPresence != NO_OPTIONS) {
                            ServiceWorkerPaymentAppBridge.Option testOption =
                                    new ServiceWorkerPaymentAppBridge.Option();
                            testOption.id = "new";
                            testOption.label = "Create BobPay account";
                            testOption.enabledMethods =
                                Arrays.asList("https://bobpay.com", "basic-card");
                            testManifest.options.add(testOption);
                        }

                        if (instrumentPresence == TWO_OPTIONS) {
                            ServiceWorkerPaymentAppBridge.Option testOption =
                                    new ServiceWorkerPaymentAppBridge.Option();
                            testOption.id = "existing";
                            testOption.label = "Existing BobPay account";
                            testOption.enabledMethods =
                                Arrays.asList("https://bobpay.com", "basic-card");
                            testManifest.options.add(testOption);
                        }

                        return Arrays.asList(testManifest);
                    }
                });
    }

    @Override
    public void onMainActivityStarted() throws InterruptedException, ExecutionException,
            TimeoutException {}

    @MediumTest
    @Feature({"Payments"})
    public void testNoOptions() throws InterruptedException, ExecutionException,
            TimeoutException {
        installServiceWorkerPaymentApp(NO_OPTIONS);
        openPageAndClickBuyAndWait(mShowFailed);
        expectResultContains(
                new String[]{"show() rejected", "The payment method is not supported"});
    }

    @MediumTest
    @Feature({"Payments"})
    public void testOneOption() throws InterruptedException, ExecutionException,
            TimeoutException {
        installServiceWorkerPaymentApp(ONE_OPTION);
        triggerUIAndWait(mReadyForInput);
        // TODO(tommyt): crbug.com/669876. Expand this test as we implement more
        // service worker based payment app functionality.
    }

    @MediumTest
    @Feature({"Payments"})
    public void testTwoOptions() throws InterruptedException, ExecutionException,
            TimeoutException {
        installServiceWorkerPaymentApp(TWO_OPTIONS);
        triggerUIAndWait(mReadyForInput);
        // TODO(tommyt): crbug.com/669876. Expand this test as we implement more
        // service worker based payment app functionality.
    }
}
