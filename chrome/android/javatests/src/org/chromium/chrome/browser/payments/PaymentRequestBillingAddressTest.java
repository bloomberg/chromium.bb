// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for biling addresses.
 */
public class PaymentRequestBillingAddressTest extends PaymentRequestTestBase {
    public PaymentRequestBillingAddressTest() {
        super("payment_request_no_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "310-310-6000", "jon.doe@gmail.com", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                billingAddressId, "" /* serverId */));
    }

    /** Verifies the format of the billing address suggestions when adding a new credit card. */
    @MediumTest
    @Feature({"Payments"})
    public void testNewCardBillingAddressFormat()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, mReadyToEdit);
        setTextInCardEditorAndWait(new String[] {"5454-5454-5454-5454", "Bob"}, mEditorTextUpdate);
        setSpinnerSelectionsInCardEditorAndWait(new int[] {11, 1, 0},
                mBillingAddressChangeProcessed);
        assertTrue(getSpinnerSelectionTextInCardEditor(2).equals(
                "Jon Doe, Google, 340 Main St, Los Angeles, CA 90291, United States"));
    }

    /**
     * Verifies that the correct number of billing address suggestions are shown when adding a new
     * credit card.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfBillingAddressSuggestions()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, mReadyToEdit);

        // There should only be two suggestions, the saved address and the option to add a new
        // address.
        assertEquals(2, getSpinnerItemCountInCardEditor(2));
    }

    /**
     * Verifies that the correct number of billing address suggestions are shown when adding a new
     * credit card, even after cancelling out of adding a new billing address.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfBillingAddressSuggestions_AfterCancellingNewBillingAddress()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Add a payment method and add a new billing address.
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, mReadyToEdit);
        setTextInCardEditorAndWait(new String[] {"5454 5454 5454 5454", "Bob"}, mEditorTextUpdate);
        setSpinnerSelectionsInCardEditorAndWait(new int[] {11, 1, 1}, mReadyToEdit);

        // Cancel the creation of a new billing address.
        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyToEdit);

        // There should still only be two suggestions, the saved address and the option to add a new
        // address.
        assertEquals(2, getSpinnerItemCountInCardEditor(2));
    }
}
