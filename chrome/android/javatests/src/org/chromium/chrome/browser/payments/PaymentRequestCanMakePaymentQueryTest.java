// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for checking whether user can make a payment via either payment app or
 * a credit card. This user has a valid credit card without a billing address on file.
 */
public class PaymentRequestCanMakePaymentQueryTest extends PaymentRequestTestBase {
    public PaymentRequestCanMakePaymentQueryTest() {
        super("payment_request_can_make_payment_query_test.html");
    }

    @Override
    public void onMainActivityStarted() throws InterruptedException, ExecutionException,
            TimeoutException {
        // The user has a valid credit card without a billing address on file. This is sufficient
        // for canMakePayment() to return true.
        new AutofillTestHelper().setCreditCard(new CreditCard("", "https://example.com", true, true,
                "Jon Doe", "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                "" /* billingAddressId */, "" /* serverId */));
    }

    @MediumTest
    @Feature({"Payments"})
    public void testNoBobPayInstalled() throws InterruptedException, ExecutionException,
            TimeoutException {
        openPageAndClickBuyAndWait(mCanMakePaymentQueryResponded);
        expectResultContains(new String[]{"true"});
    }

    @MediumTest
    @Feature({"Payments"})
    public void testNoInstrumentsInFastBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installPaymentApp(NO_INSTRUMENTS, IMMEDIATE_RESPONSE);
        openPageAndClickBuyAndWait(mCanMakePaymentQueryResponded);
        expectResultContains(new String[]{"true"});
    }

    @MediumTest
    @Feature({"Payments"})
    public void testNoInstrumentsInSlowBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installPaymentApp(NO_INSTRUMENTS, DELAYED_RESPONSE);
        openPageAndClickBuyAndWait(mCanMakePaymentQueryResponded);
        expectResultContains(new String[]{"true"});
    }

    @MediumTest
    @Feature({"Payments"})
    public void testPayViaFastBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        openPageAndClickBuyAndWait(mCanMakePaymentQueryResponded);
        expectResultContains(new String[]{"true"});
    }

    @MediumTest
    @Feature({"Payments"})
    public void testPayViaSlowBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installPaymentApp(HAVE_INSTRUMENTS, DELAYED_RESPONSE);
        openPageAndClickBuyAndWait(mCanMakePaymentQueryResponded);
        expectResultContains(new String[]{"true"});
    }
}
