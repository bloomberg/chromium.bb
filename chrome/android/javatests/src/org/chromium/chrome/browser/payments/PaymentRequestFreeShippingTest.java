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
                "US", "650-253-0000", "", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                billingAddressId, "" /* serverId */));
    }

    /** Submit the shipping address to the merchant when the user clicks "Pay." */
    @MediumTest
    @Feature({"Payments"})
    public void testPay() throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickAndWait(R.id.button_primary, getReadyForUnmaskInput());
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", getReadyToUnmask());
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, getDismissed());
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
        triggerUIAndWait(getReadyToPay());
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());
        clickInShippingAddressAndWait(R.id.payments_add_option_button, getReadyToEdit());
        clickInEditorAndWait(R.id.payments_edit_done_button, getEditorValidationError());
        clickInEditorAndWait(R.id.payments_edit_cancel_button, getReadyToPay());
        clickAndWait(R.id.close_button, getDismissed());
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Add a valid address and complete the transaction. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddAddressAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());
        clickInShippingAddressAndWait(R.id.payments_add_option_button, getReadyToEdit());
        setTextInEditorAndWait(new String[] {"Bob", "Google", "1600 Amphitheatre Pkwy",
                "Mountain View", "CA", "94043", "650-253-0000"},
                getEditorTextUpdate());
        clickInEditorAndWait(R.id.payments_edit_done_button, getReadyToPay());
        clickAndWait(R.id.button_primary, getReadyForUnmaskInput());
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", getReadyToUnmask());
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, getDismissed());
        expectResultContains(new String[] {"Bob", "Google", "1600 Amphitheatre Pkwy",
                "Mountain View", "CA", "94043", "+16502530000"});
    }

    /** Change the country in the spinner, add a valid address, and complete the transaction. */
    @MediumTest
    @Feature({"Payments"})
    public void testChangeCountryAddAddressAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());
        clickInShippingAddressAndWait(R.id.payments_add_option_button, getReadyToEdit());
        setSpinnerSelectionInEditorAndWait(0 /* Afghanistan */, getReadyToEdit());
        setTextInEditorAndWait(new String[] {"Alice", "Supreme Court", "Airport Road", "Kabul",
                "1043", "650-253-0000"},
                getEditorTextUpdate());
        clickInEditorAndWait(R.id.payments_edit_done_button, getReadyToPay());
        clickAndWait(R.id.button_primary, getReadyForUnmaskInput());
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", getReadyToUnmask());
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, getDismissed());
        expectResultContains(new String[] {
                "Alice", "Supreme Court", "Airport Road", "Kabul", "1043", "+16502530000"});
    }

    /** Quickly pressing on "add address" and then [X] should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickAddAddressAndCloseShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());

        // Quickly press on "add address" and then [X].
        int callCount = getReadyToEdit().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getPaymentRequestUI()
                        .getShippingAddressSectionForTest()
                        .findViewById(R.id.payments_add_option_button)
                        .performClick();
                getPaymentRequestUI()
                        .getDialogForTest()
                        .findViewById(R.id.close_button)
                        .performClick();
            }
        });
        getReadyToEdit().waitForCallback(callCount);

        clickInEditorAndWait(R.id.payments_edit_cancel_button, getReadyToPay());
        clickAndWait(R.id.close_button, getDismissed());
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing on [X] and then "add address" should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickCloseAndAddAddressShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());

        // Quickly press on [X] and then "add address."
        int callCount = getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getPaymentRequestUI()
                        .getDialogForTest()
                        .findViewById(R.id.close_button)
                        .performClick();
                getPaymentRequestUI()
                        .getShippingAddressSectionForTest()
                        .findViewById(R.id.payments_add_option_button)
                        .performClick();
            }
        });
        getDismissed().waitForCallback(callCount);

        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing on "add address" and then "cancel" should not crash. */
    @MediumTest
    @FlakyTest(message = "crbug.com/673371")
    @Feature({"Payments"})
    public void testQuickAddAddressAndCancelShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());

        // Quickly press on "add address" and then "cancel."
        int callCount = getReadyToEdit().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getPaymentRequestUI()
                        .getShippingAddressSectionForTest()
                        .findViewById(R.id.payments_add_option_button)
                        .performClick();
                getPaymentRequestUI()
                        .getDialogForTest()
                        .findViewById(R.id.button_secondary)
                        .performClick();
            }
        });
        getReadyToEdit().waitForCallback(callCount);

        clickInEditorAndWait(R.id.payments_edit_cancel_button, getReadyToPay());
        clickAndWait(R.id.close_button, getDismissed());
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing on "cancel" and then "add address" should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickCancelAndAddAddressShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());

        // Quickly press on "cancel" and then "add address."
        int callCount = getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getPaymentRequestUI()
                        .getDialogForTest()
                        .findViewById(R.id.button_secondary)
                        .performClick();
                getPaymentRequestUI()
                        .getShippingAddressSectionForTest()
                        .findViewById(R.id.payments_add_option_button)
                        .performClick();
            }
        });
        getDismissed().waitForCallback(callCount);

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
        triggerUIAndWait(getReadyToPay());

        // Make sure that only the appropriate enum value was logged.
        for (int i = 0; i < PaymentRequestMetrics.REQUESTED_INFORMATION_MAX; ++i) {
            assertEquals((i == PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING ? 1 : 0),
                    RecordHistogram.getHistogramValueCountForTesting(
                            "PaymentRequest.RequestedInformation", i));
        }
    }
}
