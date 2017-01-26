// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests payment via Bob Pay when performing the
 * single payment app UI skip optimization.
 */
public class PaymentRequestPaymentAppUiSkipTest extends PaymentRequestTestBase {
    public PaymentRequestPaymentAppUiSkipTest() {
        super("payment_request_bobpay_ui_skip_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {}

    /**
     * If Bob Pay is supported and installed, user should be able to pay with it. Here Bob Pay
     * responds to Chrome immediately.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaFastBobPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        openPageAndClickBuyAndWait(mDismissed);
        expectResultContains(new String[] {"https://bobpay.com", "\"transaction\"", "1337"});
    }

    /**
     * If Bob Pay is supported and installed, user should be able to pay with it. Here Bob Pay
     * responds to Chrome after a slight delay.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaSlowBobPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        installPaymentApp(HAVE_INSTRUMENTS, DELAYED_RESPONSE);
        openPageAndClickBuyAndWait(mDismissed);
        expectResultContains(new String[] {"https://bobpay.com", "\"transaction\"", "1337"});
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
        openPageAndClickBuyAndWait(mDismissed);
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
        openPageAndClickBuyAndWait(mDismissed);
        expectResultContains(new String[] {"https://bobpay.com", "\"transaction\"", "1337"});
    }
}
