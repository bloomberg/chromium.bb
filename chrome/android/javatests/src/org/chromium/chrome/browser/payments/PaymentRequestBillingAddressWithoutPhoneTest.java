// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for biling addresses without a phone.
 */
public class PaymentRequestBillingAddressWithoutPhoneTest extends PaymentRequestTestBase {
    /*
     * The index at which the option to add a billing address is located in the billing address
     * selection dropdown.
     */
    private static final int ADD_BILLING_ADDRESS = 2;

    /** The index of the billing address dropdown in the card editor. */
    private static final int BILLING_ADDRESS_DROPDOWN_INDEX = 2;

    public PaymentRequestBillingAddressWithoutPhoneTest() {
        super("payment_request_free_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String address_without_phone = helper.setProfile(new AutofillProfile("",
                "https://example.com", true, "Jon NoPhone", "Google", "340 Main St", "CA",
                "Los Angeles", "", "90291", "", "US", "", "jon.doe@gmail.com", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                address_without_phone, "" /* serverId */));
        String address_with_phone = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Rob Phone", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "310-310-6000", "jon.doe@gmail.com", "en-US"));

        // Assign use stats so that the address without a phone number has a higher frecency score.
        helper.setProfileUseStatsForTesting(address_without_phone, 10, 10);
        helper.setProfileUseStatsForTesting(address_with_phone, 5, 5);
    }

    @MediumTest
    @Feature({"Payments"})
    public void testCanPayWithBillingNoPhone()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(new String[] {"Jon NoPhone"});
    }

    @MediumTest
    @Feature({"Payments"})
    public void testCanSelectBillingAddressWithoutPhone()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);

        // Go edit the credit card.
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickOnPaymentMethodSuggestionEditIconAndWait(0, mReadyToEdit);

        // Make sure that the currently selected address is valid and can be selected (does not
        // include error messages).
        assertTrue(getSpinnerSelectionTextInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX)
                           .equals("Jon NoPhone, 340 Main St, Los Angeles, CA 90291"));

        // Even though the current billing address is valid, the one with a phone number should be
        // suggested first if the user wants to change it.
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                0).equals("Rob Phone, 340 Main St, Los Angeles, CA 90291"));
    }

    @MediumTest
    @Feature({"Payments"})
    public void testCantSelectShippingAddressWithoutPhone()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);

        // The first suggestion should be the address with a phone.
        assertTrue(getShippingAddressSuggestionLabel(0).contains("Rob Phone"));
        assertFalse(getShippingAddressSuggestionLabel(0).endsWith("Phone number required"));

        // The address without a phone should be suggested after with a message indicating that the
        // phone number is required.
        assertTrue(getShippingAddressSuggestionLabel(1).contains("Jon NoPhone"));
        assertTrue(getShippingAddressSuggestionLabel(1).endsWith("Phone number required"));
    }

    @MediumTest
    @Feature({"Payments"})
    public void testCantAddNewBillingAddressWithoutPhone()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, mReadyToEdit);

        // Add a new billing address without a phone.
        setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, ADD_BILLING_ADDRESS}, mReadyToEdit);
        setTextInEditorAndWait(
                new String[] {"Seb Doe", "Google", "340 Main St", "Los Angeles", "CA", "90291", ""},
                mEditorTextUpdate);

        // Trying to add the address without a phone number should fail.
        clickInEditorAndWait(R.id.payments_edit_done_button, mEditorValidationError);
    }
}
