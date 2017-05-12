// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.support.test.filters.MediumTest;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests a phone number and provides free shipping
 * regardless of address.
 */
public class PaymentRequestPhoneAndFreeShippingTest extends PaymentRequestTestBase {
    public PaymentRequestPhoneAndFreeShippingTest() {
        // This merchant requests a phone number and provides free shipping worldwide.
        super("payment_request_phone_and_free_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address with a valid phone number on disk.
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                billingAddressId, "" /* serverId */));
    }

    /** Submit the phone number and shipping address to the merchant when the user clicks "Pay." */
    @MediumTest
    @Feature({"Payments"})
    public void testPay() throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickAndWait(R.id.button_primary, getReadyForUnmaskInput());
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", getReadyToUnmask());
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, getDismissed());
        expectResultContains(new String[] {"+15555555555", "Jon Doe", "4111111111111111", "12",
                "2050", "visa", "123", "Google", "340 Main St", "CA", "Los Angeles", "90291", "US",
                "en", "freeShippingOption"});
    }

    /**
     * Test that starting a payment request that requires a phone number and a shipping address
     * results in the appropriate metric being logged in the PaymentRequest.RequestedInformation
     * histogram.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testRequestedInformationMetric() throws InterruptedException, ExecutionException,
            TimeoutException {
        // Start the Payment Request.
        triggerUIAndWait(getReadyToPay());

        // Make sure that only the appropriate enum value was logged.
        for (int i = 0; i < PaymentRequestMetrics.REQUESTED_INFORMATION_MAX; ++i) {
            assertEquals((i == (PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                    | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING) ? 1 : 0),
                    RecordHistogram.getHistogramValueCountForTesting(
                            "PaymentRequest.RequestedInformation", i));
        }
    }
}
