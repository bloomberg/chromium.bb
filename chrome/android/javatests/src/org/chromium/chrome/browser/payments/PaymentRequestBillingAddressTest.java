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
    /*
     * The index at which the option to add a billing address is located in the billing address
     * selection dropdown.
     */
    private static final int ADD_BILLING_ADDRESS = 3;

    /** The index of the billing address dropdown in the card editor. */
    private static final int BILLING_ADDRESS_DROPDOWN_INDEX = 2;

    public PaymentRequestBillingAddressTest() {
        super("payment_request_free_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String profile1 = helper.setProfile(new AutofillProfile("", "https://example.com", true,
                "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "", "US",
                "310-310-6000", "jon.doe@gmail.com", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa, profile1,
                "" /* serverId */));
        String profile2 = helper.setProfile(new AutofillProfile("", "https://example.com", true,
                "Rob Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "", "US",
                "310-310-6000", "jon.doe@gmail.com", "en-US"));
        String profile3 = helper.setProfile(new AutofillProfile("", "https://example.com", true,
                "Tom Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "", "US",
                "310-310-6000", "jon.doe@gmail.com", "en-US"));

        // Assign use stats so that profile2 has the highest frecency and profile3 has the lowest.
        helper.setProfileUseStatsForTesting(profile1, 5, 5);
        helper.setProfileUseStatsForTesting(profile2, 10, 10);
        helper.setProfileUseStatsForTesting(profile3, 1, 1);
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
        setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS},
                mBillingAddressChangeProcessed);
        // The billing address suggestions should include only the name, address, city, state and
        // zip code of the profile.
        assertTrue(getSpinnerSelectionTextInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX)
                           .equals("Rob Doe, 340 Main St, Los Angeles, CA 90291"));
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

        // There should only be 4 suggestions, the 3 saved addresses and the option to add a new
        // address.
        assertEquals(4, getSpinnerItemCountInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX));
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

        // Select the "+ ADD ADDRESS" option for the billing address.
        setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, ADD_BILLING_ADDRESS}, mReadyToEdit);

        // Cancel the creation of a new billing address.
        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyToEdit);

        // There should still only be 4 suggestions, the 3 saved addresses and the option to add a
        // new address.
        assertEquals(4, getSpinnerItemCountInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX));
    }

    /**
     * Verifies that the billing address suggestions are ordered by frecency.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testBillingAddressSortedByFrecency()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Add a payment method.
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, mReadyToEdit);

        // There should be 4 suggestions, the 3 saved addresses and the option to add a new address.
        assertEquals(4, getSpinnerItemCountInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX));

        // The billing address suggestions should be ordered by frecency.
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                0).equals("Rob Doe, 340 Main St, Los Angeles, CA 90291"));
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                1).equals("Jon Doe, 340 Main St, Los Angeles, CA 90291"));
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                2).equals("Tom Doe, 340 Main St, Los Angeles, CA 90291"));
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                3).equals("Add address"));
    }

    /**
     * Verifies that the billing address suggestions are ordered by frecency, except for a newly
     * created address which should be suggested first.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testBillingAddressSortedByFrecency_AddNewAddress()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Add a payment method.
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, mReadyToEdit);

        // Add a new billing address.
        setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, ADD_BILLING_ADDRESS}, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"Seb Doe", "Google", "340 Main St", "Los Angeles",
                "CA", "90291", "999-999-9999"}, mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mReadyToEdit);

        // There should be 5 suggestions, the 3 initial addresses, the newly added address and the
        // option to add a new address.
        assertEquals(5, getSpinnerItemCountInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX));

        // The fist suggestion should be the newly added address.
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                0).equals("Seb Doe, 340 Main St, Los Angeles, CA 90291"));

        // The rest of the billing address suggestions should be ordered by frecency.
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                1).equals("Rob Doe, 340 Main St, Los Angeles, CA 90291"));
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                2).equals("Jon Doe, 340 Main St, Los Angeles, CA 90291"));
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                3).equals("Tom Doe, 340 Main St, Los Angeles, CA 90291"));
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                4).equals("Add address"));
    }

    /**
     * Verifies that a newly created shipping address is offered as the first billing address
     * suggestion.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNewShippingAddressSuggestedFirst()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);

        // Add a shipping address.
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);
        clickInShippingAddressAndWait(R.id.payments_add_option_button, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"Seb Doe", "Google", "340 Main St", "Los Angeles",
                "CA", "90291", "999-999-9999"}, mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);

        // Navigate to the card editor UI.
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, mReadyToEdit);

        // There should be 5 suggestions, the 3 initial addresses, the newly added address and the
        // option to add a new address.
        assertEquals(5, getSpinnerItemCountInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX));

        // The new address should be suggested first.
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                0).equals("Seb Doe, 340 Main St, Los Angeles, CA 90291"));
    }
}
