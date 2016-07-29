// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;

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
    public void testNoSupportedPaymentMethods() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(mShowFailed);
        expectResultContains(
                new String[]{"show() rejected", "The payment method is not supported"});
    }

    /**
     * If Bob Pay does not have any instruments, reject the show() promise. Here Bob Pay responds to
     * Chrome immediately.
     */
    @MediumTest
    public void testNoInstrumentsInFastBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installPaymentApp(NO_INSTRUMENTS, IMMEDIATE_RESPONSE);
        triggerUIAndWait(mShowFailed);
        expectResultContains(
                new String[]{"show() rejected", "The payment method is not supported"});
    }

    /**
     * If Bob Pay does not have any instruments, reject the show() promise. Here Bob Pay responds to
     * Chrome after a slight delay.
     */
    @MediumTest
    public void testNoInstrumentsInSlowBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installPaymentApp(NO_INSTRUMENTS, DELAYED_RESPONSE);
        triggerUIAndWait(mShowFailed);
        expectResultContains(
                new String[]{"show() rejected", "The payment method is not supported"});
    }

    /**
     * If the payment app responds with more instruments after the UI has been dismissed, don't
     * crash.
     */
    @MediumTest
    public void testPaymentWithInstrumentsAppResponseAfterDismissShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        final BobPay app = installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
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
    public void testPaymentAppNoInstrumentsResponseAfterDismissShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        final BobPay app = installPaymentApp(NO_INSTRUMENTS, IMMEDIATE_RESPONSE);
        triggerUIAndWait(mShowFailed);
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
    public void testPayViaSlowBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installPaymentApp(HAVE_INSTRUMENTS, DELAYED_RESPONSE);
        triggerUIAndWait(mReadyToPay);
        clickAndWait(R.id.button_primary, mDismissed);
        expectResultContains(new String[]{"https://bobpay.com", "\"transaction\"", "1337"});
    }
}
