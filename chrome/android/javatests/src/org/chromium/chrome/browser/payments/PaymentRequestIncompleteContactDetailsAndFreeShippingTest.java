// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests a payer name, an email address and
 * a phone number and provides free shipping regardless of address, with the user having
 * incomplete information.
 */
public class PaymentRequestIncompleteContactDetailsAndFreeShippingTest
        extends PaymentRequestTestBase {
    public PaymentRequestIncompleteContactDetailsAndFreeShippingTest() {
        // This merchant requests an email address and a phone number and provides free shipping
        // worldwide.
        super("payment_request_contact_details_and_free_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address with a valid email address on disk. However the phone
        // number is invalid.
        helper.setProfile(new AutofillProfile("", "https://example.com", true, "Jon Doe", "Google",
                "340 Main St", "CA", "Los Angeles", "", "90291", "", "US",
                "" /* invalid phone number */, "jon.doe@google.com", "en-US"));

        installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
    }

    /** Update the shipping address with valid data and see that the contacts section is updated. */
    @MediumTest
    @Feature({"Payments"})
    public void testEditIncompleteShippingAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyForInput());
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());

        assertEquals("Jon Doe\njon.doe@google.com\nPhone number required",
                getContactDetailsSuggestionLabel(0));

        assertTrue(getShippingAddressSuggestionLabel(0).contains("Phone number required"));
        clickInShippingAddressAndWait(R.id.payments_first_radio_button, getReadyToEdit());
        setTextInEditorAndWait(new String[] {"Jon Doe", "Google", "340 Main St", "Los Angeles",
                "CA", "90291", "650-253-0000"},
                getEditorTextUpdate());
        // The contact is now complete, but not selected.
        clickInEditorAndWait(R.id.payments_edit_done_button, getReadyForInput());
        // We select it.
        clickInContactInfoAndWait(R.id.payments_section, getReadyForInput());
        assertEquals("Jon Doe\n+1 650-253-0000\njon.doe@google.com",
                getContactDetailsSuggestionLabel(0));
        clickInContactInfoAndWait(R.id.payments_first_radio_button, getReadyToPay());

        clickAndWait(R.id.button_primary, getDismissed());
        expectResultContains(new String[] {"Jon Doe", "+16502530000", "jon.doe@google.com"});
    }

    /** Add a shipping address with valid data and see that the contacts section is updated. */
    @MediumTest
    @Feature({"Payments"})
    public void testEditIncompleteShippingAndContactAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyForInput());
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());

        // There is an incomplete contact.
        assertEquals("Jon Doe\njon.doe@google.com\nPhone number required",
                getContactDetailsSuggestionLabel(0));

        // Add a new Shipping Address and see that the contact section updates.
        clickInShippingAddressAndWait(R.id.payments_add_option_button, getReadyToEdit());
        setTextInEditorAndWait(new String[] {"Jane Doe", "Edge Corp.", "111 Wall St.", "New York",
                "NY", "10110", "650-253-0000"},
                getEditorTextUpdate());
        clickInEditorAndWait(R.id.payments_edit_done_button, getReadyForInput());
        assertEquals("Jon Doe\njon.doe@google.com\nPhone number required",
                getContactDetailsSuggestionLabel(0));
        assertEquals(
                "Jane Doe\n+1 650-253-0000\nEmail required", getContactDetailsSuggestionLabel(1));

        // Now edit the first contact and pay.
        clickInContactInfoAndWait(R.id.payments_section, getReadyForInput());
        clickInContactInfoAndWait(R.id.payments_first_radio_button, getReadyToEdit());
        setTextInEditorAndWait(new String[] {"Jon Doe", "650-253-0000", "jon.doe@google.com"},
                getEditorTextUpdate());
        clickInEditorAndWait(R.id.payments_edit_done_button, getReadyToPay());

        clickAndWait(R.id.button_primary, getDismissed());
        expectResultContains(new String[] {"Jon Doe", "+16502530000", "jon.doe@google.com"});
    }
}
