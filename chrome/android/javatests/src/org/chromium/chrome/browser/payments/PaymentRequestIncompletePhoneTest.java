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
 * A payment integration test for a merchant that requests phone number from a user that has
 * incomplete phone number stored on disk.
 */
public class PaymentRequestIncompletePhoneTest extends PaymentRequestTestBase {
    public PaymentRequestIncompletePhoneTest() {
        // This merchant requests a phone number. We set the user's phone as invalid, so all tests
        // start in "ready for input" state.
        super("payment_request_phone_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has an invalid phone number on disk.
        helper.setProfile(new AutofillProfile("", "https://example.com", true, "Jon Doe", "Google",
                "340 Main St", "CA", "Los Angeles", "", "90291", "", "US",
                "+++" /* invalid phone */, "jon.doe@gmail.com", "en-US"));

        installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
    }

    /** Attempt to update the phone number with invalid data and cancel the transaction. */
    @MediumTest
    @Feature({"Payments"})
    public void testEditIncompletePhoneAndCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Not ready to pay since Contact phone is invalid.
        triggerUIAndWait(mReadyForInput);
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Contact Details).
        expectPaymentMethodRowIsSelected(0);
        // Updating contact with an invalid value and cancelling means we're still not
        // ready to pay (the value is still the original value).
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);
        clickInContactInfoAndWait(R.id.payments_first_radio_button, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"---"}, mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mEditorValidationError);
        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyForInput);
        assertEquals(PaymentRequestSection.EDIT_BUTTON_SELECT, getContactDetailsButtonState());

        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Attempt to add an invalid phone alongside the already invalid data and cancel. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddIncompletePhoneAndCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Not ready to pay since Contact phone is invalid.
        triggerUIAndWait(mReadyForInput);
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Contact Details).
        expectPaymentMethodRowIsSelected(0);
        // Updating contact with an invalid value and cancelling means we're still not
        // ready to pay (the value is still the original value).
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);
        clickInContactInfoAndWait(R.id.payments_add_option_button, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"---"}, mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mEditorValidationError);
        // The section collapses and the [SELECT] button is active.
        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyForInput);
        assertEquals(PaymentRequestSection.EDIT_BUTTON_SELECT, getContactDetailsButtonState());

        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Update the phone with valid data and provide that to the merchant. */
    @MediumTest
    @Feature({"Payments"})
    public void testEditIncompletePhoneAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);
        clickInContactInfoAndWait(R.id.payments_first_radio_button, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"514-555-5555"}, mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);
        clickAndWait(R.id.button_primary, mDismissed);
        expectResultContains(new String[] {"514-555-5555"});
    }
}
