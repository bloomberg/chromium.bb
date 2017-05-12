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
 * A payment integration test for shipping address labels.
 */
public class PaymentRequestShippingAddressTest extends PaymentRequestTestBase {
    public PaymentRequestShippingAddressTest() {
        super("payment_request_free_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address associated with a credit card.
        String firstAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                firstAddressId, "" /* serverId */));

        // The user has a second address.
        String secondAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Fred Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));

        // Set the fist profile to have a better frecency score that the second one.
        helper.setProfileUseStatsForTesting(firstAddressId, 10, 10);
        helper.setProfileUseStatsForTesting(secondAddressId, 0, 0);
    }

    /** Verifies that the shipping address format in bottomsheet mode is as expected. */
    @MediumTest
    @Feature({"Payments"})
    public void testShippingAddressFormat_BottomSheet()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());

        // Make sure that the shipping label on the bottomsheet does not include the country.
        assertTrue(getShippingAddressOptionRowAtIndex(0).getLabelText().toString().equals(
                "Jon Doe\nGoogle, 340 Main St, Los Angeles, CA 90291\n555-555-5555"));
    }

    /** Verifies that the shipping address format in fullsheet mode is as expected. */
    @MediumTest
    @Feature({"Payments"})
    public void testShippingAddressFormat_FullSheet()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());

        // Focus on a section other that shipping addresses to enter fullsheet mode.
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());

        // Make sure that the shipping label on the fullsheet does not include the country.
        assertTrue(getShippingAddressOptionRowAtIndex(0).getLabelText().toString().equals(
                "Jon Doe\nGoogle, 340 Main St, Los Angeles, CA 90291\n555-555-5555"));
    }

    /** Verifies that the shipping address format in fullsheet mode is as expected. */
    @MediumTest
    @Feature({"Payments"})
    public void testShippingAddressFormat_Expanded()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());

        // Focus on the shipping addresses section to enter expanded mode.
        clickInShippingAddressAndWait(R.id.payments_section, getReadyForInput());

        // Make sure that the shipping label in expanded mode includes the country.
        assertTrue(getShippingAddressOptionRowAtIndex(0).getLabelText().toString().equals(
                "Jon Doe\n"
                + "Google, 340 Main St, Los Angeles, CA 90291, United States\n"
                + "555-555-5555"));

        // Make sure that the second profile's shipping label also includes the country.
        assertTrue(getShippingAddressOptionRowAtIndex(1).getLabelText().toString().equals(
                "Fred Doe\n"
                + "Google, 340 Main St, Los Angeles, CA 90291, United States\n"
                + "555-555-5555"));
    }

    /** Verifies that the shipping address format of a new address is as expected. */
    @MediumTest
    @Feature({"Payments"})
    public void testShippingAddressFormat_NewAddress()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());

        // Add a shipping address.
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());
        clickInShippingAddressAndWait(R.id.payments_add_option_button, getReadyToEdit());
        setTextInEditorAndWait(new String[] {"Seb Doe", "Google", "340 Main St", "Los Angeles",
                "CA", "90291", "650-253-0000"},
                getEditorTextUpdate());
        clickInEditorAndWait(R.id.payments_edit_done_button, getReadyToPay());

        // Make sure that the shipping label does not include the country.
        assertTrue(getShippingAddressOptionRowAtIndex(0).getLabelText().toString().equals(
                "Seb Doe\nGoogle, 340 Main St, Los Angeles, CA 90291\n+1 650-253-0000"));
    }

    /**
     * Test that going into the editor and clicking 'CANCEL' button to cancel editor will leave the
     * row checked.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testEditShippingAddressAndCancelEditorShouldKeepAddressSelected()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());
        expectShippingAddressRowIsSelected(0);
        clickInShippingAddressAndWait(R.id.payments_open_editor_pencil_button, getReadyToEdit());

        // Cancel the editor by clicking 'CANCEL' button in the editor view.
        clickInEditorAndWait(R.id.payments_edit_cancel_button, getReadyToPay());

        // Expect the row to still be selected in the Shipping Address section.
        expectShippingAddressRowIsSelected(0);
    }

    /**
     * Test that going into the editor and clicking Android back button to cancel editor will leave
     * the row checked.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testEditShippingAddressAndClickAndroidBackButtonShouldKeepAddressSelected()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());
        expectShippingAddressRowIsSelected(0);
        clickInShippingAddressAndWait(R.id.payments_open_editor_pencil_button, getReadyToEdit());

        // Cancel the editor by clicking Android back button.
        clickAndroidBackButtonInEditorAndWait(getReadyToPay());

        // Expect the row to still be selected in the Shipping Address section.
        expectShippingAddressRowIsSelected(0);
    }

    /**
     * Test that going into the "add" flow  and clicking 'CANCEL' button to cancel editor will
     * leave the existing row checked.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testAddShippingAddressAndCancelEditorShouldKeepAddressSelected()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());
        expectShippingAddressRowIsSelected(0);
        clickInShippingAddressAndWait(R.id.payments_add_option_button, getReadyToEdit());

        // Cancel the editor by clicking 'CANCEL' button in the editor view.
        clickInEditorAndWait(R.id.payments_edit_cancel_button, getReadyToPay());

        // Expect the existing row to still be selected in the Shipping Address section.
        expectShippingAddressRowIsSelected(0);
    }

    /**
     * Test that going into the "add" flow  and clicking Android back button to cancel editor will
     * leave the existing row checked.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testAddShippingAddressAndClickAndroidBackButtonShouldKeepAddressSelected()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());
        expectShippingAddressRowIsSelected(0);
        clickInShippingAddressAndWait(R.id.payments_add_option_button, getReadyToEdit());

        // Cancel the editor by clicking Android back button.
        clickAndroidBackButtonInEditorAndWait(getReadyToPay());

        // Expect the existing row to still be selected in the Shipping Address section.
        expectShippingAddressRowIsSelected(0);
    }
}
