// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test to validate the logging of Payment Request metrics.
 */
public class PaymentRequestMetricsTest extends PaymentRequestTestBase {
    public PaymentRequestMetricsTest() {
        super("payment_request_free_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper mHelper = new AutofillTestHelper();
        // The user has a shipping address and a credit card associated with that address on disk.
        String mBillingAddressId = mHelper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));
        mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                mBillingAddressId));
    }

    /**
     * Expect that the successful checkout funnel metrics are logged during a succesful checkout.
     */
    @MediumTest
    public void testSuccessCheckoutFunnel() throws InterruptedException, ExecutionException,
            TimeoutException {
        // Initiate a payment request.
        triggerUIAndWait(mReadyToPay);

        // Make sure sure that the "Initiated" and "Shown" events were logged.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.CheckoutFunnel.Initiated", 1));
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.CheckoutFunnel.Shown", 1));

        // Click the pay button.
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);

        // Make sure sure that the "PayClicked" event was logged.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.CheckoutFunnel.PayClicked", 1));

        // Unmask the credit card,
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);

        // Make sure sure that the "ReceivedInstrumentDetails" and "Completed" events were logged.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.CheckoutFunnel.ReceivedInstrumentDetails", 1));
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.CheckoutFunnel.Completed", 1));
    }
}
