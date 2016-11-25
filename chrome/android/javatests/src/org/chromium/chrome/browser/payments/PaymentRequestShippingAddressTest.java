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
        triggerUIAndWait(mReadyToPay);

        // Make sure that the shipping label on the bottomsheet does not include the country.
        assertTrue(getShippingAddressOptionRowAtIndex(0).getLabelText().toString().equals(
                "Jon Doe\nGoogle, 340 Main St, Los Angeles, CA 90291\n555-555-5555"));
    }

    /** Verifies that the shipping address format in fullsheet mode is as expected. */
    @MediumTest
    @Feature({"Payments"})
    public void testShippingAddressFormat_FullSheet()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);

        // Focus on a section other that shipping addresses to enter fullsheet mode.
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);

        // Make sure that the shipping label on the fullsheet does not include the country.
        assertTrue(getShippingAddressOptionRowAtIndex(0).getLabelText().toString().equals(
                "Jon Doe\nGoogle, 340 Main St, Los Angeles, CA 90291\n555-555-5555"));
    }

    /** Verifies that the shipping address format in fullsheet mode is as expected. */
    @MediumTest
    @Feature({"Payments"})
    public void testShippingAddressFormat_Expanded()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);

        // Focus on the shipping addresses section to enter expanded mode.
        clickInShippingAddressAndWait(R.id.payments_section, mReadyForInput);

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
        triggerUIAndWait(mReadyToPay);

        // Add a shipping address.
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);
        clickInShippingAddressAndWait(R.id.payments_add_option_button, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"Seb Doe", "Google", "340 Main St", "Los Angeles",
                "CA", "90291", "555-555-5555"}, mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);

        // Make sure that the shipping label does not include the country.
        assertTrue(getShippingAddressOptionRowAtIndex(0).getLabelText().toString().equals(
                "Seb Doe\nGoogle, 340 Main St, Los Angeles, CA 90291\n555-555-5555"));
    }
}
