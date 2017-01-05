// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests contact details.
 */
public class PaymentRequestContactDetailsTest extends PaymentRequestTestBase {
    public PaymentRequestContactDetailsTest() {
        // The merchant requests a payer name, a phone number and an email address.
        super("payment_request_contact_details_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has valid payer name, phone number and email address on disk.
        helper.setProfile(new AutofillProfile("", "https://example.com", true, "Jon Doe", "Google",
                "340 Main St", "CA", "Los Angeles", "", "90291", "", "US", "555-555-5555",
                "jon.doe@google.com", "en-US"));

        // Add the same profile but with a different address.
        helper.setProfile(new AutofillProfile("", "https://example.com", true, "", "Google",
                "999 Main St", "CA", "Los Angeles", "", "90291", "", "US", "555-555-5555",
                "jon.doe@google.com", "en-US"));

        // Add the same profile but without a phone number.
        helper.setProfile(new AutofillProfile("", "https://example.com", true, "Jon Doe", "Google",
                "340 Main St", "CA", "Los Angeles", "", "90291", "", "US", "" /* phone_number */,
                "jon.doe@google.com", "en-US"));

        // Add the same profile but without an email.
        helper.setProfile(new AutofillProfile("", "https://example.com", true, "Jon Doe", "Google",
                "340 Main St", "CA", "Los Angeles", "", "90291", "", "US", "555-555-5555",
                "" /* emailAddress */, "en-US"));

        // Add the same profile but without a name.
        helper.setProfile(new AutofillProfile("" /* name */, "https://example.com", true, "",
                "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "", "US", "555-555-5555",
                "jon.doe@google.com", "en-US"));

        installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
    }

    /** Provide the existing valid payer name, phone number and email address to the merchant. */
    @MediumTest
    @Feature({"Payments"})
    public void testPay() throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickAndWait(R.id.button_primary, mDismissed);
        expectResultContains(new String[] {"Jon Doe", "555-555-5555", "jon.doe@google.com"});
    }

    /** Attempt to add invalid contact information and cancel the transaction. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddInvalidContactAndCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);
        clickInContactInfoAndWait(R.id.payments_add_option_button, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"", "+++", "jane.jones"}, mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mEditorValidationError);
        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyToPay);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Add new payer name, phone number and email address and provide that to the merchant. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddContactAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);
        clickInContactInfoAndWait(R.id.payments_add_option_button, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"Jane Jones", "999-999-9999", "jane.jones@google.com"},
                mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);

        clickAndWait(R.id.button_primary, mDismissed);
        expectResultContains(new String[] {"Jane Jones", "999-999-9999", "jane.jones@google.com"});
    }

    /** Quickly pressing on "add contact info" and then [X] should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickAddContactAndCloseShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);

        // Quickly press on "add contact info" and then [X].
        int callCount = mReadyToEdit.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getContactDetailsSectionForTest().findViewById(
                        R.id.payments_add_option_button).performClick();
                mUI.getDialogForTest().findViewById(R.id.close_button).performClick();
            }
        });
        mReadyToEdit.waitForCallback(callCount);

        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyToPay);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing on [X] and then "add contact info" should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickCloseAndAddContactShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);

        // Quickly press on [X] and then "add contact info."
        int callCount = mDismissed.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getDialogForTest().findViewById(R.id.close_button).performClick();
                mUI.getContactDetailsSectionForTest().findViewById(
                        R.id.payments_add_option_button).performClick();
            }
        });
        mDismissed.waitForCallback(callCount);

        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Test that going into the editor and cancelling will leave the row checked. */
    @MediumTest
    @Feature({"Payments"})
    public void testEditContactAndCancelEditorShouldKeepContactSelected()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);
        expectContactDetailsRowIsSelected(0);
        clickInContactInfoAndWait(R.id.payments_open_editor_pencil_button, mReadyToEdit);

        // Cancel the editor.
        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyToPay);

        // Expect the row to still be selected in the Contact Details section.
        expectContactDetailsRowIsSelected(0);
    }

    /** Test that going into the "add" flow and cancelling will leave existing row checked. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddContactAndCancelEditorShouldKeepContactSelected()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);
        expectContactDetailsRowIsSelected(0);
        clickInContactInfoAndWait(R.id.payments_add_option_button, mReadyToEdit);

        // Cancel the editor.
        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyToPay);

        // Expect the existing row to still be selected in the Contact Details section.
        expectContactDetailsRowIsSelected(0);
    }

    /** Quickly pressing on "add contact info" and then "cancel" should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickAddContactAndCancelShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);

        // Quickly press on "add contact info" and then "cancel."
        int callCount = mReadyToEdit.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getContactDetailsSectionForTest().findViewById(
                        R.id.payments_add_option_button).performClick();
                mUI.getDialogForTest().findViewById(R.id.button_secondary).performClick();
            }
        });
        mReadyToEdit.waitForCallback(callCount);

        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyToPay);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing on "cancel" and then "add contact info" should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickCancelAndAddContactShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);

        // Quickly press on "cancel" and then "add contact info."
        int callCount = mDismissed.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getDialogForTest().findViewById(R.id.button_secondary).performClick();
                mUI.getContactDetailsSectionForTest().findViewById(
                        R.id.payments_add_option_button).performClick();
            }
        });
        mDismissed.waitForCallback(callCount);

        expectResultContains(new String[] {"Request cancelled"});
    }

    /**
     * Makes sure that suggestions that are equal to or subsets of other suggestions are not shown
     * to the user.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testSuggestionsDeduped()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);
        assertEquals(1, getNumberOfContactDetailSuggestions());
    }

    /**
     * Test that starting a payment request that requires the user's email address, phone number and
     * name results in the appropriate metric being logged in the
     * PaymentRequest.RequestedInformation histogram.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testRequestedInformationMetric() throws InterruptedException, ExecutionException,
            TimeoutException {
        // Start the Payment Request.
        triggerUIAndWait(mReadyToPay);

        // Make sure that only the appropriate enum value was logged.
        for (int i = 0; i < PaymentRequestMetrics.REQUESTED_INFORMATION_MAX; ++i) {
            assertEquals((i == (PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                    | PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                    | PaymentRequestMetrics.REQUESTED_INFORMATION_NAME) ? 1 : 0),
                    RecordHistogram.getHistogramValueCountForTesting(
                            "PaymentRequest.RequestedInformation", i));
        }
    }
}
