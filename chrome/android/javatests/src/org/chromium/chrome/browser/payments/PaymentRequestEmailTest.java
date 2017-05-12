// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests email address.
 */
public class PaymentRequestEmailTest extends PaymentRequestTestBase {
    public PaymentRequestEmailTest() {
        // This merchant request an email address.
        super("payment_request_email_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a valid email address on disk.
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

    /** Provide the existing valid email address to the merchant. */
    @MediumTest
    @Feature({"Payments"})
    public void testPay() throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickAndWait(R.id.button_primary, getDismissed());
        expectResultContains(new String[] {"jon.doe@google.com"});
    }

    /** Attempt to add an invalid email address and cancel the transaction. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddInvalidEmailAndCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInContactInfoAndWait(R.id.payments_section, getReadyForInput());
        clickInContactInfoAndWait(R.id.payments_add_option_button, getReadyToEdit());
        setTextInEditorAndWait(new String[] {"jane.jones"}, getEditorTextUpdate());
        clickInEditorAndWait(R.id.payments_edit_done_button, getEditorValidationError());
        clickInEditorAndWait(R.id.payments_edit_cancel_button, getReadyToPay());
        clickAndWait(R.id.close_button, getDismissed());
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Add a new email address and provide that to the merchant. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddEmailAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInContactInfoAndWait(R.id.payments_section, getReadyForInput());
        clickInContactInfoAndWait(R.id.payments_add_option_button, getReadyToEdit());
        setTextInEditorAndWait(new String[] {"jane.jones@google.com"}, getEditorTextUpdate());
        clickInEditorAndWait(R.id.payments_edit_done_button, getReadyToPay());

        clickAndWait(R.id.button_primary, getDismissed());
        expectResultContains(new String[] {"jane.jones@google.com"});
    }

    /**
     * Makes sure that suggestions that are equal to or subsets of other suggestions are not shown
     * to the user.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testSuggestionsDeduped()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInContactInfoAndWait(R.id.payments_section, getReadyForInput());
        assertEquals(1, getNumberOfContactDetailSuggestions());
    }

    /**
     * Test that starting a payment request that requires only the user's email address results in
     * the appropriate metric being logged in the PaymentRequest.RequestedInformation histogram.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testRequestedInformationMetric() throws InterruptedException, ExecutionException,
            TimeoutException {
        // Start the Payment Request.
        triggerUIAndWait(getReadyToPay());

        // Make sure that only the appropriate enum value was logged.
        for (int i = 0; i < PaymentRequestMetrics.REQUESTED_INFORMATION_MAX; ++i) {
            assertEquals((i == PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL ? 1 : 0),
                    RecordHistogram.getHistogramValueCountForTesting(
                            "PaymentRequest.RequestedInformation", i));
        }
    }
}
