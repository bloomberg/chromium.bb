// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for checking whether user can make active payment via a credit card.
 * This user does not have a complete credit card on file.
 */
@CommandLineFlags.Add("enable-blink-features=CanMakeActivePayment")
public class PaymentRequestCcActivePaymentQueryNoCardTest extends PaymentRequestTestBase {
    public PaymentRequestCcActivePaymentQueryNoCardTest() {
        super("payment_request_active_payment_query_cc_test.html");
    }

    @Override
    public void onMainActivityStarted() throws InterruptedException, ExecutionException,
            TimeoutException {
        // The user has an incomplete credit card on file. This is not sufficient for
        // canMakeActivePayment() to return true.
        new AutofillTestHelper().setCreditCard(new CreditCard("", "https://example.com", true, true,
                "Jon Doe", "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                "" /* billingAddressId */, "" /* serverId */));
    }

    @MediumTest
    @Feature({"Payments"})
    public void testCannotMakeActivePayment() throws InterruptedException, ExecutionException,
            TimeoutException {
        openPageAndClickBuyAndWait(mActivePaymentQueryResponded);
        expectResultContains(new String[]{"false"});
    }
}
