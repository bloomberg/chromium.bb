// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.os.Build;
import android.support.test.filters.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that does not require shipping address.
 */
public class PaymentRequestNoShippingTest extends PaymentRequestTestBase {
    public PaymentRequestNoShippingTest() {
        super("payment_request_no_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "650-253-0000", "jon.doe@gmail.com", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                billingAddressId, "" /* serverId */));
    }

    /** Click [X] to cancel payment. */
    @MediumTest
    @Feature({"Payments"})
    public void testCloseDialog() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(getReadyForInput());
        clickAndWait(R.id.close_button, getDismissed());
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Click [EDIT] to expand the dialog, then click [X] to cancel payment. */
    @MediumTest
    @Feature({"Payments"})
    public void testEditAndCloseDialog() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(getReadyForInput());
        clickAndWait(R.id.button_secondary, getReadyForInput());
        clickAndWait(R.id.close_button, getDismissed());
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Click [EDIT] to expand the dialog, then click [CANCEL] to cancel payment. */
    @MediumTest
    @Feature({"Payments"})
    public void testEditAndCancelDialog() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(getReadyForInput());
        clickAndWait(R.id.button_secondary, getReadyForInput());
        clickAndWait(R.id.button_secondary, getDismissed());
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Click [PAY] and dismiss the card unmask dialog. */
    @MediumTest
    @Feature({"Payments"})
    public void testPay() throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickAndWait(R.id.button_primary, getReadyForUnmaskInput());
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", getReadyToUnmask());
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, getDismissed());
        expectResultContains(new String[] {"Jon Doe", "4111111111111111", "12", "2050", "visa",
                "123"});
    }

    /** Click [PAY], type in "123" into the CVC dialog, then submit the payment. */
    @MediumTest
    @Feature({"Payments"})
    public void testCancelUnmaskAndRetry()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickAndWait(R.id.button_primary, getReadyForUnmaskInput());
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_NEGATIVE, getReadyToPay());
        clickAndWait(R.id.button_primary, getReadyForUnmaskInput());
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", getReadyToUnmask());
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, getDismissed());
        expectResultContains(new String[] {"Jon Doe", "4111111111111111", "12", "2050", "visa",
                "123"});
    }

    /** Attempt to add an invalid credit card number and cancel payment. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddInvalidCardNumberAndCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Attempting to add an invalid card and cancelling out of the flow will result in the user
        // still being ready to pay with the previously selected credit card.
        fillNewCardForm("123", "Bob", DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS);
        clickInCardEditorAndWait(R.id.payments_edit_done_button, getEditorValidationError());
        clickInCardEditorAndWait(R.id.payments_edit_cancel_button, getReadyToPay());
        clickAndWait(R.id.close_button, getDismissed());
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Attempt to add a credit card with an empty name on card and cancel payment. */
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP) // crbug.com/678983
    @Feature({"Payments"})
    public void testAddEmptyNameOnCardAndCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        fillNewCardForm("5454-5454-5454-5454", "", DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS);
        clickInCardEditorAndWait(R.id.payments_edit_done_button, getEditorValidationError());
        clickInCardEditorAndWait(R.id.payments_edit_cancel_button, getReadyForInput());
        clickAndWait(R.id.close_button, getDismissed());
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Save a new card on disk and pay. */
    @MediumTest
    @Feature({"Payments"})
    public void testSaveNewCardAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        fillNewCardForm("5454-5454-5454-5454", "Bob", DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS);
        clickInCardEditorAndWait(R.id.payments_edit_done_button, getReadyToPay());
        clickAndWait(R.id.button_primary, getReadyForUnmaskInput());
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", getReadyToUnmask());
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, getDismissed());
        expectResultContains(new String[] {"5454545454545454", "12", "Bob"});
    }

    /** Use a temporary credit card to complete payment. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddTemporaryCardAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        fillNewCardForm("5454-5454-5454-5454", "Bob", DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS);

        // Uncheck the "Save this card on this device" checkbox, so the card is temporary.
        selectCheckboxAndWait(R.id.payments_edit_checkbox, false, getReadyToEdit());

        clickInCardEditorAndWait(R.id.payments_edit_done_button, getReadyToPay());
        clickAndWait(R.id.button_primary, getReadyForUnmaskInput());
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", getReadyToUnmask());
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, getDismissed());
        expectResultContains(new String[] {"5454545454545454", "12", "Bob"});
    }

    private void fillNewCardForm(String cardNumber, String nameOnCard, int month, int year,
            int billingAddress) throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInPaymentMethodAndWait(R.id.payments_section, getReadyForInput());
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, getReadyToEdit());
        setTextInCardEditorAndWait(new String[] {cardNumber, nameOnCard}, getEditorTextUpdate());
        setSpinnerSelectionsInCardEditorAndWait(
                new int[] {month, year, billingAddress}, getBillingAddressChangeProcessed());
    }

    /** Add a new card together with a new billing address and pay. */
    @MediumTest
    @Feature({"Payments"})
    public void testSaveNewCardAndNewBillingAddressAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInPaymentMethodAndWait(R.id.payments_section, getReadyForInput());
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, getReadyToEdit());
        setTextInCardEditorAndWait(
                new String[] {"5454 5454 5454 5454", "Bob"}, getEditorTextUpdate());

        // Select December of next year for expiration and [Add address] in the billing address
        // dropdown.
        int addBillingAddress = 1;
        setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, addBillingAddress}, getReadyToEdit());

        setTextInEditorAndWait(new String[] {"Bob", "Google", "1600 Amphitheatre Pkwy",
                "Mountain View", "CA", "94043", "650-253-0000"},
                getEditorTextUpdate());
        clickInEditorAndWait(R.id.payments_edit_done_button, getReadyToEdit());

        clickInCardEditorAndWait(R.id.payments_edit_done_button, getReadyToPay());
        clickAndWait(R.id.button_primary, getReadyForUnmaskInput());
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", getReadyToUnmask());
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, getDismissed());
        expectResultContains(new String[] {"5454545454545454", "12", "Bob", "Google",
                "1600 Amphitheatre Pkwy", "Mountain View", "CA", "94043", "+16502530000"});
    }

    /** Quickly pressing on "add card" and then [X] should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickAddCardAndCloseShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInPaymentMethodAndWait(R.id.payments_section, getReadyForInput());

        // Quickly press on "add card" and then [X].
        int callCount = getReadyToEdit().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getPaymentRequestUI()
                        .getPaymentMethodSectionForTest()
                        .findViewById(R.id.payments_add_option_button)
                        .performClick();
                getPaymentRequestUI()
                        .getDialogForTest()
                        .findViewById(R.id.close_button)
                        .performClick();
            }
        });
        getReadyToEdit().waitForCallback(callCount);

        clickInCardEditorAndWait(R.id.payments_edit_cancel_button, getReadyToPay());
        clickAndWait(R.id.close_button, getDismissed());
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing on [X] and then "add card" should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickCloseAndAddCardShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInPaymentMethodAndWait(R.id.payments_section, getReadyForInput());

        // Quickly press on [X] and then "add card."
        int callCount = getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getPaymentRequestUI()
                        .getDialogForTest()
                        .findViewById(R.id.close_button)
                        .performClick();
                getPaymentRequestUI()
                        .getPaymentMethodSectionForTest()
                        .findViewById(R.id.payments_add_option_button)
                        .performClick();
            }
        });
        getDismissed().waitForCallback(callCount);

        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing on "add card" and then "cancel" should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickAddCardAndCancelShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInPaymentMethodAndWait(R.id.payments_section, getReadyForInput());

        // Quickly press on "add card" and then "cancel."
        int callCount = getReadyToEdit().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getPaymentRequestUI()
                        .getPaymentMethodSectionForTest()
                        .findViewById(R.id.payments_add_option_button)
                        .performClick();
                getPaymentRequestUI()
                        .getDialogForTest()
                        .findViewById(R.id.button_secondary)
                        .performClick();
            }
        });
        getReadyToEdit().waitForCallback(callCount);

        clickInCardEditorAndWait(R.id.payments_edit_cancel_button, getReadyToPay());
        clickAndWait(R.id.close_button, getDismissed());
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing on "cancel" and then "add card" should not crash. */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickCancelAndAddCardShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInPaymentMethodAndWait(R.id.payments_section, getReadyForInput());

        // Quickly press on "cancel" and then "add card."
        int callCount = getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getPaymentRequestUI()
                        .getDialogForTest()
                        .findViewById(R.id.button_secondary)
                        .performClick();
                getPaymentRequestUI()
                        .getPaymentMethodSectionForTest()
                        .findViewById(R.id.payments_add_option_button)
                        .performClick();
            }
        });
        getDismissed().waitForCallback(callCount);

        expectResultContains(new String[] {"Request cancelled"});
    }

    /**
     * Quickly dismissing the dialog (via Android's back button, for example) and then pressing on
     * "pay" should not crash.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickDismissAndPayShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());

        // Quickly dismiss and then press on "pay."
        int callCount = getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getPaymentRequestUI().getDialogForTest().onBackPressed();
                getPaymentRequestUI()
                        .getDialogForTest()
                        .findViewById(R.id.button_primary)
                        .performClick();
            }
        });
        getDismissed().waitForCallback(callCount);

        expectResultContains(new String[] {"Request cancelled"});
    }

    /**
     * Quickly dismissing the dialog (via Android's back button, for example) and then pressing on
     * [X] should not crash.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickDismissAndCloseShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());

        // Quickly dismiss and then press on [X].
        int callCount = getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getPaymentRequestUI().getDialogForTest().onBackPressed();
                getPaymentRequestUI()
                        .getDialogForTest()
                        .findViewById(R.id.close_button)
                        .performClick();
            }
        });
        getDismissed().waitForCallback(callCount);

        expectResultContains(new String[] {"Request cancelled"});
    }

    /**
     * Quickly pressing on [X] and then dismissing the dialog (via Android's back button, for
     * example) should not crash.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testQuickCloseAndDismissShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(getReadyToPay());

        // Quickly press on [X] and then dismiss.
        int callCount = getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getPaymentRequestUI()
                        .getDialogForTest()
                        .findViewById(R.id.close_button)
                        .performClick();
                getPaymentRequestUI().getDialogForTest().onBackPressed();
            }
        });
        getDismissed().waitForCallback(callCount);

        expectResultContains(new String[] {"Request cancelled"});
    }

    /**
     * Test that starting a payment request that requires user information except for the payment
     * results in the appropriate metric being logged in the PaymentRequest.RequestedInformation
     * histogram.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testRequestedInformationMetric() throws InterruptedException, ExecutionException,
            TimeoutException {
        // Start the Payment Request.
        triggerUIAndWait(getReadyToPay());

        // Make sure that only the appropriate enum value was logged.
        for (int i = 0; i < PaymentRequestMetrics.REQUESTED_INFORMATION_MAX; ++i) {
            assertEquals((i == PaymentRequestMetrics.REQUESTED_INFORMATION_NONE ? 1 : 0),
                    RecordHistogram.getHistogramValueCountForTesting(
                            "PaymentRequest.RequestedInformation", i));
        }
    }
}
