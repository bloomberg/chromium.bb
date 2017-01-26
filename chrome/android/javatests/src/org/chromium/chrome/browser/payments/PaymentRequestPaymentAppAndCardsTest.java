// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests payment via Bob Pay or cards.
 */
public class PaymentRequestPaymentAppAndCardsTest extends PaymentRequestTestBase {
    public PaymentRequestPaymentAppAndCardsTest() {
        super("payment_request_bobpay_and_cards_test.html");
    }

    @Override
    public void onMainActivityStarted() throws InterruptedException, ExecutionException,
            TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "310-310-6000", "jon.doe@gmail.com", "en-US"));
        // Mastercard card without a billing address.
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "5454545454545454", "", "12", "2050", "mastercard", R.drawable.pr_mc,
                "" /* billingAddressId */, "" /* serverId */));
        // Visa card with complete set of information.
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "", "12", "2050", "visa", R.drawable.pr_visa, billingAddressId,
                "" /* serverId */));
    }

    /**
     * If Bob Pay does not have any instruments, show [visa, mastercard]. Here the payment app
     * responds quickly.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNoInstrumentsInFastBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        runTest(NO_INSTRUMENTS, IMMEDIATE_RESPONSE);
    }

    /**
     * If Bob Pay does not have any instruments, show [visa, mastercard]. Here the payment app
     * responds slowly.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNoInstrumentsInSlowBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        runTest(NO_INSTRUMENTS, DELAYED_RESPONSE);
    }

    /**
     * If Bob Pay has instruments, show [bobpay, visa, mastercard]. Here the payment app responds
     * quickly.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testHaveInstrumentsInFastBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        runTest(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
    }

    /**
     * If Bob Pay has instruments, show [bobpay, visa, mastercard]. Here the payment app responds
     * slowly.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testHaveInstrumentsInSlowBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        runTest(HAVE_INSTRUMENTS, DELAYED_RESPONSE);
    }

    /** Test that going into the editor and cancelling will leave the row checked. */
    @MediumTest
    @Feature({"Payments"})
    public void testEditPaymentMethodAndCancelEditorShouldKeepCardSelected()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        expectPaymentMethodRowIsSelected(0);
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, mReadyToEdit);

        // Cancel the editor.
        clickInCardEditorAndWait(R.id.payments_edit_cancel_button, mReadyToPay);

        // Expect the existing row to still be selected in the Shipping Address section.
        expectPaymentMethodRowIsSelected(0);
    }

    /** Test that going into "add" flow editor and cancelling will leave existing row checked. */
    @MediumTest
    @Feature({"Payments"})
    public void testAddPaymentMethodAndCancelEditorShouldKeepExistingCardSelected()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        expectPaymentMethodRowIsSelected(0);
        clickInPaymentMethodAndWait(R.id.payments_open_editor_pencil_button, mReadyToEdit);

        // Cancel the editor.
        clickInCardEditorAndWait(R.id.payments_edit_cancel_button, mReadyToPay);

        // Expect the row to still be selected in the Shipping Address section.
        expectPaymentMethodRowIsSelected(0);
    }

    private void runTest(int instrumentPresence, int responseSpeed) throws InterruptedException,
            ExecutionException, TimeoutException  {
        installPaymentApp(instrumentPresence, responseSpeed);
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);

        // Check the number of instruments.
        assertEquals(
                instrumentPresence == HAVE_INSTRUMENTS ? 3 : 2, getNumberOfPaymentInstruments());

        // Check the labels of the instruments.
        int i = 0;
        if (instrumentPresence == HAVE_INSTRUMENTS) {
            assertEquals("https://bobpay.com", getPaymentInstrumentLabel(i++));
        }
        // \u0020\...\u2006 is four dots ellipsis.
        assertEquals(
                "Visa\u0020\u0020\u2022\u2006\u2022\u2006\u2022\u2006\u2022\u20061111\nJon Doe",
                getPaymentInstrumentLabel(i++));
        assertEquals(
                "MasterCard\u0020\u0020\u2022\u2006\u2022\u2006\u2022\u2006\u2022\u20065454\n"
                + "Jon Doe\nBilling address required",
                getPaymentInstrumentLabel(i++));

        // Check the output of the selected instrument.
        if (instrumentPresence == HAVE_INSTRUMENTS) {
            clickAndWait(R.id.button_primary, mDismissed);
            expectResultContains(new String[]{"https://bobpay.com", "\"transaction\"", "1337"});
        } else {
            clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
            setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
            clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
            expectResultContains(new String[] {"Jon Doe", "4111111111111111", "12", "2050", "visa",
                    "123"});
        }
    }
}
