// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests a payer name and provides free shipping
 * regardless of address.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
public class PaymentRequestNameAndFreeShippingTest implements MainActivityStartCallback {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_name_and_free_shipping_test.html", this);

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address with a valid payer name on disk.
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                CardType.UNKNOWN, billingAddressId, "" /* serverId */));
    }

    /** Submit the payer name and shipping address to the merchant when the user clicks "Pay." */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPay() throws InterruptedException, ExecutionException, TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                DialogInterface.BUTTON_POSITIVE, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"Jon Doe", "Jon Doe",
                "4111111111111111", "12", "2050", "visa", "123", "Google", "340 Main St", "CA",
                "Los Angeles", "90291", "US", "en", "freeShippingOption"});
    }

    /**
     * Test that ending a payment request that requires a payer name and a shipping address
     * results in the appropriate metric being logged in PaymentRequest.Events.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentRequestEventsMetric()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Start and cancel the Payment Request.
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"Request cancelled"});

        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_PAYER_NAME | Event.REQUEST_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }
}
