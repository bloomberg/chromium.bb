// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for the different fields trials affecting payment requests.
 */
public class PaymentRequestFieldTrialTest extends PaymentRequestTestBase {
    public PaymentRequestFieldTrialTest() {
        // This merchant supports bobpay and credit cards payments.
        super("payment_request_bobpay_and_cards_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address on disk but no credit card.
        helper.setProfile(new AutofillProfile("", "https://example.com", true, "Jon Doe", "Google",
                "340 Main St", "CA", "Los Angeles", "", "90291", "", "US", "555-555-5555", "",
                "en-US"));
    }

    /**
     * Tests that the Payment Request is cancelled if the user has no credit card and the proper
     * command line flag is set.
     */
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add("enable-features=NoCreditCardAbort")
    public void testAbortIfNoCard_Enabled_NoApp()
            throws InterruptedException, ExecutionException, TimeoutException {
        openPageAndClickBuyAndWait(mShowFailed);
        expectResultContains(new String[] {"The payment method is not supported"});
    }

    /**
     * Tests that the Payment Request UI is shown even if the user has no credit card and the proper
     * command line flag is set, if they have another payment app that is accepted by the merchant.
     */
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add("enable-features=NoCreditCardAbort")
    public void testAbortIfNoCard_Enabled_WithApp()
            throws InterruptedException, ExecutionException, TimeoutException {
        installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        triggerUIAndWait(mReadyToPay);
    }

    /**
     * Tests that the Payment Request UI is shown if the user has no credit card and the proper
     * command line flag is set.
     */
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add("disable-features=NoCreditCardAbort")
    public void testAbortIfNoCard_Disabled()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Check that the Payment Request UI is shown.
        triggerUIAndWait(mReadyForInput);
    }
}
