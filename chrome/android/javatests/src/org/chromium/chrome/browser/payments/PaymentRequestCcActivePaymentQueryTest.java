// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for checking whether user can make active payment via a credit card.
 * This user has a complete credit card on file.
 */
@CommandLineFlags.Add("enable-blink-features=CanMakeActivePayment")
public class PaymentRequestCcActivePaymentQueryTest extends PaymentRequestTestBase {
    public PaymentRequestCcActivePaymentQueryTest() {
        super("payment_request_active_payment_query_cc_test.html");
    }

    @Override
    public void onMainActivityStarted() throws InterruptedException, ExecutionException,
            TimeoutException {
        // The user has a complete credit card on file. This is sufficient for
        // canMakeActivePayment() to return true.
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                billingAddressId, "" /* serverId */));
    }

    @MediumTest
    @Feature({"Payments"})
    public void testCanMakeActivePayment() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(mActivePaymentQueryResponded);
        expectResultContains(new String[]{"true"});

        // Repeating a query does not count against the quota.
        clickNodeAndWait("buy", mActivePaymentQueryResponded);
        expectResultContains(new String[]{"true"});

        clickNodeAndWait("buy", mActivePaymentQueryResponded);
        expectResultContains(new String[]{"true"});

        // Different queries are throttled for a period of time.
        clickNodeAndWait("other-buy", mActivePaymentQueryResponded);
        expectResultContains(new String[]{"Query quota exceeded"});

        // Repeating the same query again does not count against the quota.
        clickNodeAndWait("buy", mActivePaymentQueryResponded);
        expectResultContains(new String[]{"true"});
    }
}
