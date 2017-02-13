// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.payments.PaymentAppFactory.PaymentAppCreatedCallback;
import org.chromium.chrome.browser.payments.PaymentAppFactory.PaymentAppFactoryAddition;
import org.chromium.content_public.browser.WebContents;

import java.util.Arrays;
import java.util.Set;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests payment via Bob Pay.
 */
public class PaymentRequestPaymentAppTest extends PaymentRequestTestBase {
    public PaymentRequestPaymentAppTest() {
        super("payment_request_bobpay_test.html");
    }

    @Override
    public void onMainActivityStarted() throws InterruptedException, ExecutionException,
            TimeoutException {}

    /** If no payment methods are supported, reject the show() promise. */
    @MediumTest
    @Feature({"Payments"})
    public void testNoSupportedPaymentMethods() throws InterruptedException, ExecutionException,
            TimeoutException {
        openPageAndClickBuyAndWait(mShowFailed);
        expectResultContains(
                new String[]{"show() rejected", "The payment method is not supported"});
    }

    /**
     * If Bob Pay does not have any instruments, reject the show() promise. Here Bob Pay responds to
     * Chrome immediately.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNoInstrumentsInFastBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installPaymentApp(NO_INSTRUMENTS, IMMEDIATE_RESPONSE);
        openPageAndClickBuyAndWait(mShowFailed);
        expectResultContains(
                new String[]{"show() rejected", "The payment method is not supported"});
    }

    /**
     * If Bob Pay does not have any instruments, reject the show() promise. Here Bob Pay responds to
     * Chrome after a slight delay.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNoInstrumentsInSlowBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installPaymentApp(NO_INSTRUMENTS, DELAYED_RESPONSE);
        openPageAndClickBuyAndWait(mShowFailed);
        expectResultContains(
                new String[]{"show() rejected", "The payment method is not supported"});
    }

    /**
     * If the payment app responds with more instruments after the UI has been dismissed, don't
     * crash.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentWithInstrumentsAppResponseAfterDismissShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        final TestPay app = new TestPay(
                Arrays.asList("https://bobpay.com"), HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        PaymentAppFactory.getInstance().addAdditionalFactory(new PaymentAppFactoryAddition() {
            @Override
            public void create(WebContents webContents, Set<String> methodNames,
                    PaymentAppFactory.PaymentAppCreatedCallback callback) {
                callback.onPaymentAppCreated(app);
                callback.onAllPaymentAppsCreated();
            }
        });
        triggerUIAndWait(mReadyForInput);
        clickAndWait(R.id.close_button, mDismissed);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                app.respond();
            }
        });
        expectResultContains(new String[]{"show() rejected", "Request cancelled"});
    }

    /**
     * If the payment app responds with no instruments after the UI has been dismissed, don't crash.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentAppNoInstrumentsResponseAfterDismissShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        final TestPay app = new TestPay(
                Arrays.asList("https://bobpay.com"), NO_INSTRUMENTS, IMMEDIATE_RESPONSE);
        PaymentAppFactory.getInstance().addAdditionalFactory(new PaymentAppFactoryAddition() {
            @Override
            public void create(WebContents webContents, Set<String> methodNames,
                    PaymentAppCreatedCallback callback) {
                callback.onPaymentAppCreated(app);
                callback.onAllPaymentAppsCreated();
            }
        });
        openPageAndClickBuyAndWait(mShowFailed);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                app.respond();
            }
        });
        expectResultContains(
                new String[]{"show() rejected", "The payment method is not supported"});
    }

    /**
     * If Bob Pay is supported and installed, user should be able to pay with it. Here Bob Pay
     * responds to Chrome immediately.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaFastBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        triggerUIAndWait(mReadyToPay);
        clickAndWait(R.id.button_primary, mDismissed);
        expectResultContains(new String[]{"https://bobpay.com", "\"transaction\"", "1337"});
    }

    /**
     * If Bob Pay is supported and installed, user should be able to pay with it. Here Bob Pay
     * responds to Chrome after a slight delay.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaSlowBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installPaymentApp(HAVE_INSTRUMENTS, DELAYED_RESPONSE);
        triggerUIAndWait(mReadyToPay);
        clickAndWait(R.id.button_primary, mDismissed);
        expectResultContains(new String[]{"https://bobpay.com", "\"transaction\"", "1337"});
    }

    /**
     * Test payment with a Bob Pay that is created with a delay, but responds immediately
     * to getInstruments.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaDelayedFastBobPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        installPaymentApp(
                "https://bobpay.com", HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE, DELAYED_CREATION);
        triggerUIAndWait(mReadyToPay);
        clickAndWait(R.id.button_primary, mDismissed);
        expectResultContains(new String[] {"https://bobpay.com", "\"transaction\"", "1337"});
    }

    /**
     * Test payment with a Bob Pay that is created with a delay, and responds slowly to
     * getInstruments.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaDelayedSlowBobPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        installPaymentApp(
                "https://bobpay.com", HAVE_INSTRUMENTS, DELAYED_RESPONSE, DELAYED_CREATION);
        triggerUIAndWait(mReadyToPay);
        clickAndWait(R.id.button_primary, mDismissed);
        expectResultContains(new String[] {"https://bobpay.com", "\"transaction\"", "1337"});
    }
}
