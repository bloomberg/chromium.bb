// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for "basic-card" payment method.
 */
public class PaymentRequestBasicCardTest extends PaymentRequestTestBase {
    public PaymentRequestBasicCardTest() {
        super("payment_request_basic_card_test.html");
        PaymentRequestImpl.setIsLocalCanMakePaymentQueryQuotaEnforcedForTest();
    }

    @Override
    public void onMainActivityStarted() throws InterruptedException, ExecutionException,
            TimeoutException {
        // The user has a valid "visa" card.
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                CardType.UNKNOWN, billingAddressId, "" /* serverId */));
    }

    @MediumTest
    @Feature({"Payments"})
    public void testCanPayWithBasicCard() throws InterruptedException,
            ExecutionException, TimeoutException {
        openPageAndClickNodeAndWait("checkBasicCard", getCanMakePaymentQueryResponded());
        expectResultContains(new String[] {"true"});

        clickNodeAndWait("buyBasicCard", getReadyForInput());
    }

    @MediumTest
    @Feature({"Payments"})
    public void testIgnoreCardType() throws InterruptedException,
            ExecutionException, TimeoutException {
        openPageAndClickNodeAndWait("checkBasicDebit", getCanMakePaymentQueryResponded());
        expectResultContains(new String[] {"true"});

        clickNodeAndWait("buyBasicDebit", getReadyForInput());
    }

    @MediumTest
    @Feature({"Payments"})
    public void testCannotMakeActivePaymentWithBasicMasterCard() throws InterruptedException,
            ExecutionException, TimeoutException {
        openPageAndClickNodeAndWait("checkBasicMasterCard", getCanMakePaymentQueryResponded());
        expectResultContains(new String[] {"false"});

        reTriggerUIAndWait("buyBasicMasterCard", getReadyForInput());
    }

    @MediumTest
    @Feature({"Payments"})
    public void testSupportedNetworksMustMatchForCanMakePayment()
            throws InterruptedException, ExecutionException, TimeoutException {
        openPageAndClickNodeAndWait("checkBasicVisa", getCanMakePaymentQueryResponded());
        expectResultContains(new String[] {"true"});

        clickNodeAndWait("checkBasicMasterCard", getCanMakePaymentQueryResponded());
        expectResultContains(new String[] {"Not allowed to check whether can make payment"});

        clickNodeAndWait("checkBasicVisa", getCanMakePaymentQueryResponded());
        expectResultContains(new String[] {"true"});
    }

    @MediumTest
    @Feature({"Payments"})
    public void testSupportedTypesMustMatchForCanMakePayment()
            throws InterruptedException, ExecutionException, TimeoutException {
        openPageAndClickNodeAndWait("checkBasicVisa", getCanMakePaymentQueryResponded());
        expectResultContains(new String[] {"true"});

        clickNodeAndWait("checkBasicDebit", getCanMakePaymentQueryResponded());
        expectResultContains(new String[] {"Not allowed to check whether can make payment"});

        clickNodeAndWait("checkBasicVisa", getCanMakePaymentQueryResponded());
        expectResultContains(new String[] {"true"});
    }

    /**
     * If the merchant requests supported methods of "mastercard" and "basic-card" with "visa"
     * network support, then the user should be able to pay via their "visa" card. The merchant will
     * receive the "basic-card" method name.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testPayWithBasicCard()  throws InterruptedException, ExecutionException,
            TimeoutException {
        openPageAndClickNodeAndWait("checkBasicVisa", getCanMakePaymentQueryResponded());
        expectResultContains(new String[] {"true"});

        reTriggerUIAndWait("buy", getReadyToPay());
        clickAndWait(R.id.button_primary, getReadyForUnmaskInput());
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", getReadyToUnmask());
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, getDismissed());
        expectResultContains(new String[] {"Jon Doe", "4111111111111111", "12", "2050",
                "basic-card", "123"});
    }

    /**
     * If the merchant requests supported methods of "mastercard" and "basic-card" with "visa"
     * network support, then the user should be able to add a "mastercard" card and pay with it. The
     * merchant will receive the "mastercard" method name.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testAddMasterCard()  throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInPaymentMethodAndWait(R.id.payments_section, getReadyForInput());
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, getReadyToEdit());
        setTextInCardEditorAndWait(
                new String[] {"5555-5555-5555-4444", "Jane Jones"}, getEditorTextUpdate());
        setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS},
                getBillingAddressChangeProcessed());
        clickInCardEditorAndWait(R.id.payments_edit_done_button, getReadyToPay());
        clickAndWait(R.id.button_primary, getReadyForUnmaskInput());
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", getReadyToUnmask());
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, getDismissed());
        expectResultContains(
                new String[] {"5555555555554444", "12", "Jane Jones", "123", "mastercard"});
    }

    /**
     * If the merchant requests supported methods of "mastercard" and "basic-card" with "visa"
     * network support, then the user should be able to add a new "visa" card and pay with it. The
     * merchant will receive the "basic-card" method name.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testAddBasicCard()  throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(getReadyToPay());
        clickInPaymentMethodAndWait(R.id.payments_section, getReadyForInput());
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, getReadyToEdit());
        setTextInCardEditorAndWait(
                new String[] {"4242-4242-4242-4242", "Jane Jones"}, getEditorTextUpdate());
        setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS},
                getBillingAddressChangeProcessed());
        clickInCardEditorAndWait(R.id.payments_edit_done_button, getReadyToPay());
        clickAndWait(R.id.button_primary, getReadyForUnmaskInput());
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", getReadyToUnmask());
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, getDismissed());
        expectResultContains(
                new String[] {"4242424242424242", "12", "Jane Jones", "123", "basic-card"});
    }
}
