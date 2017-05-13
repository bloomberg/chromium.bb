// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
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
                        List<PaymentInstrument> instruments = new ArrayList<PaymentInstrument>();

                        if (instrumentPresence != NO_OPTIONS) {
                            instruments.add(new ServiceWorkerPaymentInstrument(webContents,
                                    0 /* swRegistrationId */, "new" /* instrumentId */,
                                    "Create BobPay account" /* label */,
                                    new HashSet<String>(Arrays.asList("https://bobpay.com",
                                            "basic-card")) /* methodNames */));
                        }

                        if (instrumentPresence == TWO_OPTIONS) {
                            instruments.add(new ServiceWorkerPaymentInstrument(webContents,
                                    0 /* swRegistrationId */, "existing" /* instrumentId */,
                                    "Existing BobPay account" /* label */,
                                    new HashSet<String>(Arrays.asList("https://bobpay.com",
                                            "basic-card")) /* methodNames */));
                        }

                        callback.onPaymentAppCreated(
                                new ServiceWorkerPaymentApp(webContents, instruments));
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
        openPageAndClickBuyAndWait(getShowFailed());
        expectResultContains(
                new String[]{"show() rejected", "The payment method is not supported"});
    }

    @MediumTest
    @Feature({"Payments"})
    public void testOneOption() throws InterruptedException, ExecutionException,
            TimeoutException {
        installMockServiceWorkerPaymentApp(ONE_OPTION);
        triggerUIAndWait(getReadyForInput());
        // TODO(tommyt): crbug.com/669876. Expand this test as we implement more
        // service worker based payment app functionality.
    }

    @MediumTest
    @Feature({"Payments"})
    public void testTwoOptions() throws InterruptedException, ExecutionException,
            TimeoutException {
        installMockServiceWorkerPaymentApp(TWO_OPTIONS);
        triggerUIAndWait(getReadyForInput());
        // TODO(tommyt): crbug.com/669876. Expand this test as we implement more
        // service worker based payment app functionality.
    }
}
