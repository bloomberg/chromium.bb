// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.WebContents;

import java.util.Arrays;
import java.util.Set;
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
     * Installs a mock service worker based payment app for testing.
     *
     * @param instrumentPresence Whether the manifest has any payment options. Either NO_OPTIONS
     *                           ONE_OPTION or TWO_OPTIONS
     */
    private void installMockServiceWorkerPaymentApp(final int instrumentPresence) {
        PaymentAppFactory.getInstance().addAdditionalFactory(
                new PaymentAppFactory.PaymentAppFactoryAddition() {
                    @Override
                    public void create(WebContents webContents, Set<String> methodNames,
                            PaymentAppFactory.PaymentAppCreatedCallback callback) {
                        ServiceWorkerPaymentAppBridge.Manifest testManifest =
                                new ServiceWorkerPaymentAppBridge.Manifest();
                        testManifest.registrationId = 0;
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

                        callback.onPaymentAppCreated(
                                new ServiceWorkerPaymentApp(webContents, testManifest));
                        callback.onAllPaymentAppsCreated();
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
        installMockServiceWorkerPaymentApp(NO_OPTIONS);
        openPageAndClickBuyAndWait(mShowFailed);
        expectResultContains(
                new String[]{"show() rejected", "The payment method is not supported"});
    }

    @MediumTest
    @Feature({"Payments"})
    public void testOneOption() throws InterruptedException, ExecutionException,
            TimeoutException {
        installMockServiceWorkerPaymentApp(ONE_OPTION);
        triggerUIAndWait(mReadyForInput);
        // TODO(tommyt): crbug.com/669876. Expand this test as we implement more
        // service worker based payment app functionality.
    }

    @MediumTest
    @Feature({"Payments"})
    public void testTwoOptions() throws InterruptedException, ExecutionException,
            TimeoutException {
        installMockServiceWorkerPaymentApp(TWO_OPTIONS);
        triggerUIAndWait(mReadyForInput);
        // TODO(tommyt): crbug.com/669876. Expand this test as we implement more
        // service worker based payment app functionality.
    }
}
