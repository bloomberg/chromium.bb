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
                "US", "555-555-5555", "", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                billingAddressId, "" /* serverId */));
    }

    /** The shipping address should not be selected in UI by default. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddressNotSelected()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
        assertEquals(PaymentRequestSection.EDIT_BUTTON_SELECT, getSummarySectionButtonState());
    }

    /** Expand the shipping address section, select an address, and click "Pay." */
    @MediumTest
    @Feature({"Payments"})
    public void testSelectValidAddressAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Shipping Address).
        expectPaymentMethodRowIsSelected(0);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);
        clickInShippingAddressAndWait(R.id.payments_first_radio_button, mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(new String[] {"Jon Doe", "4111111111111111", "12", "2050", "visa",
                "123", "Google", "340 Main St", "CA", "Los Angeles", "90291", "US", "en",
                "californiaShippingOption"});
    }

    /** Expand the shipping address section, select an address, edit it and click "Pay." */
    @MediumTest
    @Feature({"Payments"})
    public void testSelectValidAddressEditItAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Shipping Address).
        expectPaymentMethodRowIsSelected(0);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);
        clickInShippingAddressAndWait(R.id.payments_first_radio_button, mReadyToPay);
        expectShippingAddressRowIsSelected(0);
        clickInShippingAddressAndWait(R.id.payments_open_editor_pencil_button, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"Jane Doe"}, mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);
        expectShippingAddressRowIsSelected(0);

        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(new String[] {"Jane Doe", "4111111111111111", "12", "2050", "visa",
                "123", "Google", "340 Main St", "CA", "Los Angeles", "90291", "US", "en",
                "californiaShippingOption"});
    }

    /** Expand the shipping address section, select address, edit but cancel editing, and "Pay". */
    @MediumTest
    @Feature({"Payments"})
    public void testSelectValidAddressEditItAndCancelAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Shipping Address).
        expectPaymentMethodRowIsSelected(0);
        clickInShippingAddressAndWait(R.id.payments_section, mReadyForInput);
        clickInShippingAddressAndWait(R.id.payments_first_radio_button, mReadyToPay);
        expectShippingAddressRowIsSelected(0);
        clickInShippingAddressAndWait(R.id.payments_open_editor_pencil_button, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"Jane Doe"}, mEditorTextUpdate);
        // Cancel the edit.
        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyToPay);
        expectShippingAddressRowIsSelected(0);

        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(new String[] {"Jon Doe", "4111111111111111", "12", "2050", "visa",
                "123", "Google", "340 Main St", "CA", "Los Angeles", "90291", "US", "en",
                "californiaShippingOption"});
    }

    /** Attempt to add an invalid address and cancel the transaction. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddInvalidAddressAndCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Shipping Address).
        expectPaymentMethodRowIsSelected(0);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);
        clickInShippingAddressAndWait(R.id.payments_add_option_button, mReadyToEdit);
        clickInEditorAndWait(R.id.payments_edit_done_button, mEditorValidationError);
        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyForInput);

        clickAndWait(R.id.close_button, mDismissed);
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
        triggerUIAndWait(mReadyForInput);
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

    /** Quickly pressing "add address" and then [X] should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickAddAddressAndCloseShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);

        // Quickly press "add address" and then [X].
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

        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyForInput);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing [X] and then "add address" should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickCloseAndAddAddressShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);

        // Quickly press [X] and then "add address."
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

    /** Quickly pressing "add address" and then "cancel" should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickAddAddressAndCancelShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);

        // Quickly press "add address" and then "cancel."
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

        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyForInput);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing on "cancel" and then "add address" should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickCancelAndAddAddressShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
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
}
