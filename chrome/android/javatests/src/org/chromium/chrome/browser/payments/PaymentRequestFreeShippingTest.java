// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.support.test.filters.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that provides free shipping regardless of address.
 */
public class PaymentRequestFreeShippingTest extends PaymentRequestTestBase {
    public PaymentRequestFreeShippingTest() {
        // This merchant provides free shipping worldwide.
        super("payment_request_free_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address on disk.
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                billingAddressId, "" /* serverId */));
    }

    /** Submit the shipping address to the merchant when the user clicks "Pay." */
    @MediumTest
    @Feature({"Payments"})
    public void testPay() throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(new String[] {"Jon Doe", "4111111111111111", "12", "2050", "visa",
                "123", "Google", "340 Main St", "CA", "Los Angeles", "90291", "US", "en",
                "freeShippingOption"});
    }

    /** Attempt to add an invalid address and cancel the transaction. */
    @MediumTest
    @FlakyTest(message = "crbug.com/673371")
    @Feature({"Payments"})
    public void testAddInvalidAddressAndCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);
        clickInShippingAddressAndWait(R.id.payments_add_option_button, mReadyToEdit);
        clickInEditorAndWait(R.id.payments_edit_done_button, mEditorValidationError);
        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyToPay);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Add a valid address and complete the transaction. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddAddressAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);
        clickInShippingAddressAndWait(R.id.payments_add_option_button, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"Bob", "Google", "1600 Amphitheatre Pkwy",
                "Mountain View", "CA", "94043", "999-999-9999"}, mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(new String[] {"Bob", "Google", "1600 Amphitheatre Pkwy",
                "Mountain View", "CA", "94043", "999-999-9999"});
    }

    /** Change the country in the spinner, add a valid address, and complete the transaction. */
    @MediumTest
    @Feature({"Payments"})
    public void testChangeCountryAddAddressAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);
        clickInShippingAddressAndWait(R.id.payments_add_option_button, mReadyToEdit);
        setSpinnerSelectionInEditorAndWait(0 /* Afghanistan */, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"Alice", "Supreme Court", "Airport Road", "Kabul",
                "1043", "999-999-9999"}, mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(new String[] {"Alice", "Supreme Court", "Airport Road", "Kabul",
                "1043", "999-999-9999"});
    }

    /** Quickly pressing on "add address" and then [X] should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickAddAddressAndCloseShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);

        // Quickly press on "add address" and then [X].
        int callCount = mReadyToEdit.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getShippingAddressSectionForTest().findViewById(
                        R.id.payments_add_option_button).performClick();
                mUI.getDialogForTest().findViewById(R.id.close_button).performClick();
            }
        });
        mReadyToEdit.waitForCallback(callCount);

        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyToPay);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing on [X] and then "add address" should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickCloseAndAddAddressShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);

        // Quickly press on [X] and then "add address."
        int callCount = mDismissed.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getDialogForTest().findViewById(R.id.close_button).performClick();
                mUI.getShippingAddressSectionForTest().findViewById(
                        R.id.payments_add_option_button).performClick();
            }
        });
        mDismissed.waitForCallback(callCount);

        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing on "add address" and then "cancel" should not crash. */
    @MediumTest
    @FlakyTest(message = "crbug.com/673371")
    @Feature({"Payments"})
    public void testQuickAddAddressAndCancelShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);

        // Quickly press on "add address" and then "cancel."
        int callCount = mReadyToEdit.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getShippingAddressSectionForTest().findViewById(
                        R.id.payments_add_option_button).performClick();
                mUI.getDialogForTest().findViewById(R.id.button_secondary).performClick();
            }
        });
        mReadyToEdit.waitForCallback(callCount);

        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyToPay);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing on "cancel" and then "add address" should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickCancelAndAddAddressShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);

        // Quickly press on "cancel" and then "add address."
        int callCount = mDismissed.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getDialogForTest().findViewById(R.id.button_secondary).performClick();
                mUI.getShippingAddressSectionForTest().findViewById(
                        R.id.payments_add_option_button).performClick();
            }
        });
        mDismissed.waitForCallback(callCount);

        expectResultContains(new String[] {"Request cancelled"});
    }

    /**
     * Test that starting a payment request that requires only the shipping address results in the
     * appropriate metric being logged in the PaymentRequest.RequestedInformation histogram.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testRequestedInformationMetric() throws InterruptedException, ExecutionException,
            TimeoutException {
        // Start the Payment Request.
        triggerUIAndWait(mReadyToPay);

        // Make sure that only the appropriate enum value was logged.
        for (int i = 0; i < PaymentRequestMetrics.REQUESTED_INFORMATION_MAX; ++i) {
            assertEquals((i == PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING ? 1 : 0),
                    RecordHistogram.getHistogramValueCountForTesting(
                            "PaymentRequest.RequestedInformation", i));
        }
    }
}
