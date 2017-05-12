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
 * A payment integration test for a merchant that requests email address from a user that has
 * incomplete email address stored on disk.
 */
public class PaymentRequestIncompleteEmailTest extends PaymentRequestTestBase {
    public PaymentRequestIncompleteEmailTest() {
        // This merchant requests an email address. We set the user's email as invalid, so all tests
        // start in "ready for input" state.
        super("payment_request_email_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has an invalid email address on disk.
        helper.setProfile(new AutofillProfile("", "https://example.com", true, "Jon Doe", "Google",
                "340 Main St", "CA", "Los Angeles", "", "90291", "", "US", "555-555-5555",
                "jon.doe" /* invalid email address */, "en-US"));

        installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
    }

    /** Attempt to update the email with invalid data and cancel the transaction. */
    @MediumTest
    @Feature({"Payments"})
    public void testEditIncompleteEmailAndCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Not ready to pay since Contact email is invalid.
        triggerUIAndWait(getReadyForInput());
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Contact Details).
        expectPaymentMethodRowIsSelected(0);
        // Updating contact with an invalid value and cancelling means we're still not
        // ready to pay (the value is still the original value).
        clickInContactInfoAndWait(R.id.payments_section, getReadyForInput());
        clickInContactInfoAndWait(R.id.payments_first_radio_button, getReadyToEdit());
        setTextInEditorAndWait(new String[] {"gmail.com"}, getEditorTextUpdate());
        clickInEditorAndWait(R.id.payments_edit_done_button, getEditorValidationError());
        clickInEditorAndWait(R.id.payments_edit_cancel_button, getReadyForInput());
        assertEquals(PaymentRequestSection.EDIT_BUTTON_CHOOSE, getContactDetailsButtonState());

        clickAndWait(R.id.close_button, getDismissed());
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Attempt to add an invalid email alongside the already invalid data and cancel. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddIncompleteEmailAndCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Not ready to pay since Contact email is invalid.
        triggerUIAndWait(getReadyForInput());
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Contact Details).
        expectPaymentMethodRowIsSelected(0);
        // Updating contact with an invalid value and cancelling means we're still not
        // ready to pay (the value is still the original value).
        clickInContactInfoAndWait(R.id.payments_section, getReadyForInput());
        clickInContactInfoAndWait(R.id.payments_add_option_button, getReadyToEdit());
        setTextInEditorAndWait(new String[] {"gmail.com"}, getEditorTextUpdate());
        clickInEditorAndWait(R.id.payments_edit_done_button, getEditorValidationError());
        // The section collapses and the [CHOOSE] button is active.
        clickInEditorAndWait(R.id.payments_edit_cancel_button, getReadyForInput());
        assertEquals(PaymentRequestSection.EDIT_BUTTON_CHOOSE, getContactDetailsButtonState());

        clickAndWait(R.id.close_button, getDismissed());
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Update the email with valid data and provide that to the merchant. */
    @MediumTest
    @Feature({"Payments"})
    public void testEditIncompleteEmailAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyForInput());
        clickInContactInfoAndWait(R.id.payments_section, getReadyForInput());
        clickInContactInfoAndWait(R.id.payments_first_radio_button, getReadyToEdit());
        setTextInEditorAndWait(new String[] {"jon.doe@google.com"}, getEditorTextUpdate());
        clickInEditorAndWait(R.id.payments_edit_done_button, getReadyToPay());
        clickAndWait(R.id.button_primary, getDismissed());
        expectResultContains(new String[] {"jon.doe@google.com"});
    }
}
