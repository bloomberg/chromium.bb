// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests contact details from a user that has
 * incomplete contact details stored on disk.
 */
public class PaymentRequestIncompleteContactDetailsTest extends PaymentRequestTestBase {
    public PaymentRequestIncompleteContactDetailsTest() {
        // This merchant requests both a phone number and an email address. We set the user's email
        // address as invalid, so all tests start in "ready for input" state.
        super("payment_request_contact_details_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has an invalid email address on disk.
        helper.setProfile(new AutofillProfile("", "https://example.com", true, "Jon Doe", "Google",
                "340 Main St", "CA", "Los Angeles", "", "90291", "", "US", "333-333-3333",
                "jon.doe" /* invalid email address */, "en-US"));

        installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
    }

    /** Attempt to update the contact information with invalid data and cancel the transaction. */
    @MediumTest
    @Feature({"Payments"})
    public void testEditIncompleteContactAndCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Not ready to pay since Contact email is invalid.
        triggerUIAndWait(mReadyForInput);
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Contact Details).
        expectPaymentMethodRowIsSelected(0);
        // Updating contact with an invalid value and cancelling means we're still not
        // ready to pay (the value is still the original value).
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);
        clickInContactInfoAndWait(R.id.payments_first_radio_button, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"", "---", "jane.jones"}, mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mEditorValidationError);
        // The section collapses and the [SELECT] button is active.
        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyForInput);
        assertEquals(PaymentRequestSection.EDIT_BUTTON_SELECT, getContactDetailsButtonState());

        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Attempt to add invalid contact info alongside the already invalid info, and cancel. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddIncompleteContactAndCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Not ready to pay since Contact email is invalid.
        triggerUIAndWait(mReadyForInput);
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Contact Details).
        expectPaymentMethodRowIsSelected(0);
        // Adding contact with an invalid value and cancelling means we're still not
        // ready to pay (the value is still the original value).
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);
        clickInContactInfoAndWait(R.id.payments_add_option_button, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"", "---", "jane.jones"}, mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mEditorValidationError);
        // The section collapses and the [SELECT] button is active.
        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyForInput);
        assertEquals(PaymentRequestSection.EDIT_BUTTON_SELECT, getContactDetailsButtonState());

        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Update the contact information with valid data and provide that to the merchant. */
    @MediumTest
    @Feature({"Payments"})
    public void testEditIncompleteContactAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);
        clickInContactInfoAndWait(R.id.payments_first_radio_button, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"Jon Doe", "555-555-5555", "jon.doe@google.com"},
                mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);
        clickAndWait(R.id.button_primary, mDismissed);
        expectResultContains(new String[] {"Jon Doe", "555-555-5555", "jon.doe@google.com"});
    }
}
