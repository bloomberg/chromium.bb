// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

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
    private static final int ADD_BILLING_ADDRESS = 7;

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

        // Incomplete profile (invalid address).
        String profile4 = helper.setProfile(new AutofillProfile("", "https://example.com", true,
                "Bart Doe", "Google", "340 Main St", "CA", "", "", "90291", "", "US",
                "310-310-6000", "jon.doe@gmail.com", "en-US"));

        // Incomplete profile (missing phone number)
        String profile5 = helper.setProfile(new AutofillProfile("", "https://example.com", true,
                "Lisa Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "", "US", "",
                "jon.doe@gmail.com", "en-US"));

        // Incomplete profile (missing recipient).
        String profile6 = helper.setProfile(new AutofillProfile("", "https://example.com", true, "",
                "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "", "US", "310-310-6000",
                "jon.doe@gmail.com", "en-US"));

        // Incomplete profile (need more information).
        String profile7 = helper.setProfile(new AutofillProfile("", "https://example.com", true, "",
                "Google", "340 Main St", "CA", "", "", "90291", "", "US", "", "", "en-US"));

        // Profile with empty street address (should not be presented to user).
        String profile8 = helper.setProfile(new AutofillProfile("", "https://example.com", true,
                "Jerry Doe", "Google", "" /* streetAddress */, "CA", "Los Angeles", "", "90291", "",
                "US", "310-310-6000", "jerry.doe@gmail.com", "en-US"));

        // This card has no billing address selected.
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jane Doe",
                "4242424242424242", "1111", "12", "2050", "visa", R.drawable.pr_visa, profile6,
                "" /* serverId */));

        // Assign use stats so that incomplete profiles have the highest frecency, profile2 has the
        // highest frecency and profile3 has the lowest among the complete profiles, and profile8
        // has the highest frecency and profile4 has the lowest among the incomplete profiles.
        helper.setProfileUseStatsForTesting(profile1, 5, 5);
        helper.setProfileUseStatsForTesting(profile2, 10, 10);
        helper.setProfileUseStatsForTesting(profile3, 1, 1);
        helper.setProfileUseStatsForTesting(profile4, 15, 15);
        helper.setProfileUseStatsForTesting(profile5, 30, 30);
        helper.setProfileUseStatsForTesting(profile6, 25, 25);
        helper.setProfileUseStatsForTesting(profile7, 20, 20);
        helper.setProfileUseStatsForTesting(profile8, 40, 40);
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

        // There should only be 8 suggestions, the 7 saved addresses and the option to add a new
        // address.
        assertEquals(8, getSpinnerItemCountInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX));
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

        // There should still only be 8 suggestions, the 7 saved addresses and the option to add a
        // new address.
        assertEquals(8, getSpinnerItemCountInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX));
    }

    /**
     * Tests that for a card that already has a billing address, adding a new one and cancelling
     * maintains the previous selection. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddBillingAddressOnCardAndCancel_MaintainsPreviousSelection()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        // Edit the only card.
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_open_editor_pencil_button, mReadyToEdit);

        // Jon Doe is selected as the billing address.
        assertTrue(getSpinnerSelectionTextInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX)
                .equals("Jon Doe, 340 Main St, Los Angeles, CA 90291"));

        // Select the "+ ADD ADDRESS" option for the billing address.
        setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, ADD_BILLING_ADDRESS}, mReadyToEdit);

        // Cancel the creation of a new billing address.
        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyToEdit);

        // Jon Doe is STILL selected as the billing address.
        assertTrue(getSpinnerSelectionTextInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX)
                .equals("Jon Doe, 340 Main St, Los Angeles, CA 90291"));
    }

    /**
     * Tests that adding a billing address for a card that has none, and cancelling then returns
     * to the proper selection (Select...).
     */
    @MediumTest
    @Feature({"Payments"})
    public void testAddBillingAddressOnCardWithNoBillingAndCancel_MaintainsPreviousSelection()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        // Edit the second card.
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickOnPaymentMethodSuggestionOptionAndWait(1, mReadyToEdit);

        // Now in Card Editor to add a billing address. "Select" is selected in the dropdown.
        assertTrue(getSpinnerSelectionTextInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX)
                .equals("Select"));

        // Select the "+ ADD ADDRESS" option for the billing address.
        setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, ADD_BILLING_ADDRESS}, mReadyToEdit);

        // Cancel the creation of a new billing address.
        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyToEdit);

        // "Select" is STILL selected as the billing address.
        assertTrue(getSpinnerSelectionTextInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX)
                .equals("Select"));
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

        // There should be 8 suggestions, the 7 saved addresses and the option to add a new address.
        assertEquals(8, getSpinnerItemCountInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX));

        // The billing address suggestions should be ordered by frecency.
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                0).equals("Rob Doe, 340 Main St, Los Angeles, CA 90291"));
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                1).equals("Jon Doe, 340 Main St, Los Angeles, CA 90291"));
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                2).equals("Tom Doe, 340 Main St, Los Angeles, CA 90291"));
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                7).equals("Add address"));
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

        // There should be 9 suggestions, the 7 initial addresses, the newly added address and the
        // option to add a new address.
        assertEquals(9, getSpinnerItemCountInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX));

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
                8).equals("Add address"));
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

        // There should be 9 suggestions, the 7 initial addresses, the newly added address and the
        // option to add a new address.
        assertEquals(9, getSpinnerItemCountInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX));

        // The new address should be suggested first.
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                0).equals("Seb Doe, 340 Main St, Los Angeles, CA 90291"));
    }

    @MediumTest
    @Feature({"Payments"})
    public void testSelectIncompleteBillingAddress_EditComplete()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        // Edit the second card.
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickOnPaymentMethodSuggestionOptionAndWait(1, mReadyToEdit);

        // Now "Select" is selected in the dropdown.
        assertTrue(getSpinnerSelectionTextInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX)
                .equals("Select"));

        // The incomplete addresses in the dropdown contain edit required messages.
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                4).endsWith("Recipient required"));
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                5).endsWith("More information required"));
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                6).endsWith("Invalid address"));

        // Selects the fourth billing addresss that misses recipient brings up the address
        // editor.
        setSpinnerSelectionsInCardEditorAndWait(new int[] {DECEMBER, NEXT_YEAR, 4}, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"Lisa Doh", "Google", "340 Main St", "Los Angeles",
                "CA", "90291", "999-999-9999"}, mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mReadyToEdit);

        // The newly completed address must be selected and put at the top of the dropdown.
        assertTrue(getSpinnerSelectionTextInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX)
                           .equals("Lisa Doh, 340 Main St, Los Angeles, CA 90291"));
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                0).equals("Lisa Doh, 340 Main St, Los Angeles, CA 90291"));
    }

    @MediumTest
    @Feature({"Payments"})
    public void testSelectIncompleteBillingAddress_EditCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        // Edit the only complete card.
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_open_editor_pencil_button, mReadyToEdit);

        // Jon Doe is selected as the billing address.
        assertTrue(getSpinnerSelectionTextInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX)
                .equals("Jon Doe, 340 Main St, Los Angeles, CA 90291"));

        // The incomplete addresses in the dropdown contain edit required messages.
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                4).endsWith("Recipient required"));
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                5).endsWith("More information required"));
        assertTrue(getSpinnerTextAtPositionInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX,
                6).endsWith("Invalid address"));

        // Selects the fifth billing addresss that misses recipient name brings up the address
        // editor.
        setSpinnerSelectionsInCardEditorAndWait(new int[] {DECEMBER, NEXT_YEAR, 4}, mReadyToEdit);
        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyToEdit);

        // The previous selected address should be selected after canceling out from edit.
        assertTrue(getSpinnerSelectionTextInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX)
                .equals("Jon Doe, 340 Main St, Los Angeles, CA 90291"));
    }
}
