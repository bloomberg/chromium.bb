// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for checking whether user can make a payment using a payment app.
 */
public class PaymentRequestPaymentAppCanMakePaymentQueryTest extends PaymentRequestTestBase {
    public PaymentRequestPaymentAppCanMakePaymentQueryTest() {
        super("payment_request_can_make_payment_query_bobpay_test.html");
    }

    @Override
    public void onMainActivityStarted() throws InterruptedException, ExecutionException,
            TimeoutException {}

    @MediumTest
    @Feature({"Payments"})
    public void testNoBobPayInstalled() throws InterruptedException, ExecutionException,
            TimeoutException {
        openPageAndClickBuyAndWait(mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"false, false"});

        clickNodeAndWait("otherBuy", mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"false, QuotaExceededError"});
    }

    @MediumTest
    @Feature({"Payments"})
    public void testBobPayInstalledLater()
            throws InterruptedException, ExecutionException, TimeoutException {
        openPageAndClickBuyAndWait(mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"false, false"});

        installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);

        clickNodeAndWait("otherBuy", mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"true, QuotaExceededError"});
    }

    @MediumTest
    @Feature({"Payments"})
    public void testNoInstrumentsInFastBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installPaymentApp(NO_INSTRUMENTS, IMMEDIATE_RESPONSE);
        openPageAndClickBuyAndWait(mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"false, false"});

        clickNodeAndWait("otherBuy", mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"false, QuotaExceededError"});
    }

    @MediumTest
    @Feature({"Payments"})
    public void testNoInstrumentsInSlowBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installPaymentApp(NO_INSTRUMENTS, DELAYED_RESPONSE);
        openPageAndClickBuyAndWait(mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"false, false"});

        clickNodeAndWait("otherBuy", mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"false, QuotaExceededError"});
    }

    @MediumTest
    @Feature({"Payments"})
    public void testPayViaFastBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        openPageAndClickBuyAndWait(mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"true, true"});

        clickNodeAndWait("otherBuy", mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"true, QuotaExceededError"});
    }

    @MediumTest
    @Feature({"Payments"})
    public void testPayViaSlowBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installPaymentApp(HAVE_INSTRUMENTS, DELAYED_RESPONSE);
        openPageAndClickBuyAndWait(mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"true, true"});

        clickNodeAndWait("otherBuy", mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"true, QuotaExceededError"});
    }
}
