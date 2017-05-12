// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.support.test.filters.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requires shipping address to calculate shipping
 * and user that has a single address stored in autofill settings.
 */
public class PaymentRequestDynamicShippingSingleAddressTest extends PaymentRequestTestBase {
    public PaymentRequestDynamicShippingSingleAddressTest() {
        // This merchant requests the shipping address first before providing any shipping options.
        super("payment_request_dynamic_shipping_test.html");
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

    /** The shipping address should not be selected in UI by default. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddressNotSelected()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyForInput());
        assertEquals(PaymentRequestSection.EDIT_BUTTON_CHOOSE, getSummarySectionButtonState());
    }

    /** Expand the shipping address section, select an address, and click "Pay." */
    @MediumTest
    @Feature({"Payments"})
    public void testSelectValidAddressAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyForInput());
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Shipping Address).
        expectPaymentMethodRowIsSelected(0);
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());
        clickInShippingAddressAndWait(R.id.payments_first_radio_button, getReadyToPay());
        clickAndWait(R.id.button_primary, getReadyForUnmaskInput());
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", getReadyToUnmask());
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, getDismissed());
        expectResultContains(new String[] {"Jon Doe", "4111111111111111", "12", "2050", "visa",
                "123", "Google", "340 Main St", "CA", "Los Angeles", "90291", "+16502530000", "US",
                "en", "californiaShippingOption"});
    }

    /** Expand the shipping address section, select an address, edit it and click "Pay." */
    @MediumTest
    @Feature({"Payments"})
    public void testSelectValidAddressEditItAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyForInput());
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Shipping Address).
        expectPaymentMethodRowIsSelected(0);
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());
        clickInShippingAddressAndWait(R.id.payments_first_radio_button, getReadyToPay());
        expectShippingAddressRowIsSelected(0);
        clickInShippingAddressAndWait(R.id.payments_open_editor_pencil_button, getReadyToEdit());
        setTextInEditorAndWait(new String[] {"Jane Doe"}, getEditorTextUpdate());
        clickInEditorAndWait(R.id.payments_edit_done_button, getReadyToPay());
        expectShippingAddressRowIsSelected(0);

        clickAndWait(R.id.button_primary, getReadyForUnmaskInput());
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", getReadyToUnmask());
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, getDismissed());
        expectResultContains(new String[] {"Jane Doe", "4111111111111111", "12", "2050", "visa",
                "123", "Google", "340 Main St", "CA", "Los Angeles", "90291", "+16502530000", "US",
                "en", "californiaShippingOption"});
    }

    /** Expand the shipping address section, select address, edit but cancel editing, and "Pay". */
    @MediumTest
    @Feature({"Payments"})
    public void testSelectValidAddressEditItAndCancelAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyForInput());
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Shipping Address).
        expectPaymentMethodRowIsSelected(0);
        clickInShippingAddressAndWait(R.id.payments_section, getReadyForInput());
        clickInShippingAddressAndWait(R.id.payments_first_radio_button, getReadyToPay());
        expectShippingAddressRowIsSelected(0);
        clickInShippingAddressAndWait(R.id.payments_open_editor_pencil_button, getReadyToEdit());
        setTextInEditorAndWait(new String[] {"Jane Doe"}, getEditorTextUpdate());
        // Cancel the edit.
        clickInEditorAndWait(R.id.payments_edit_cancel_button, getReadyToPay());
        expectShippingAddressRowIsSelected(0);

        clickAndWait(R.id.button_primary, getReadyForUnmaskInput());
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", getReadyToUnmask());
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, getDismissed());
        expectResultContains(new String[] {"Jon Doe", "4111111111111111", "12", "2050", "visa",
                "123", "Google", "340 Main St", "CA", "Los Angeles", "90291", "+16502530000", "US",
                "en", "californiaShippingOption"});
    }

    /** Attempt to add an invalid address and cancel the transaction. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddInvalidAddressAndCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyForInput());
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Shipping Address).
        expectPaymentMethodRowIsSelected(0);
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());
        clickInShippingAddressAndWait(R.id.payments_add_option_button, getReadyToEdit());
        clickInEditorAndWait(R.id.payments_edit_done_button, getEditorValidationError());
        clickInEditorAndWait(R.id.payments_edit_cancel_button, getReadyForInput());

        clickAndWait(R.id.close_button, getDismissed());
        expectResultContains(new String[] {"Request cancelled"});
    }

    /**
     * Add a valid address and complete the transaction.
     * @MediumTest
     * @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE) // crbug.com/626289
     */
    @FlakyTest(message = "crbug.com/626289")
    @Feature({"Payments"})
    public void testAddAddressAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyForInput());
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

    /** Quickly pressing "add address" and then [X] should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickAddAddressAndCloseShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyForInput());
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());

        // Quickly press "add address" and then [X].
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

        clickInEditorAndWait(R.id.payments_edit_cancel_button, getReadyForInput());
        clickAndWait(R.id.close_button, getDismissed());
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing [X] and then "add address" should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickCloseAndAddAddressShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyForInput());
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());

        // Quickly press [X] and then "add address."
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

    /** Quickly pressing "add address" and then "cancel" should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickAddAddressAndCancelShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyForInput());
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());

        // Quickly press "add address" and then "cancel."
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

        clickInEditorAndWait(R.id.payments_edit_cancel_button, getReadyForInput());
        clickAndWait(R.id.close_button, getDismissed());
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing on "cancel" and then "add address" should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickCancelAndAddAddressShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyForInput());
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
}
