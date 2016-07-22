// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.test.suitebuilder.annotation.MediumTest;

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
                ""));
        // Visa card with complete set of information.
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "", "12", "2050", "visa", R.drawable.pr_visa,
                billingAddressId));
    }

    /**
     * If Bob Pay does not have any instruments, show [visa, mastercard]. Here the payment app
     * responds quickly.
     */
    @MediumTest
    public void testNoInstrumentsInFastBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        runTest(NO_INSTRUMENTS, IMMEDIATE_RESPONSE);
    }

    /**
     * If Bob Pay does not have any instruments, show [visa, mastercard]. Here the payment app
     * responds slowly.
     */
    @MediumTest
    public void testNoInstrumentsInSlowBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        runTest(NO_INSTRUMENTS, DELAYED_RESPONSE);
    }

    /**
     * If Bob Pay has instruments, show [bobpay, visa, mastercard]. Here the payment app responds
     * quickly.
     */
    @MediumTest
    public void testHaveInstrumentsInFastBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        runTest(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
    }

    /**
     * If Bob Pay has instruments, show [bobpay, visa, mastercard]. Here the payment app responds
     * slowly.
     */
    @MediumTest
    public void testHaveInstrumentsInSlowBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        runTest(HAVE_INSTRUMENTS, DELAYED_RESPONSE);
    }

    private void runTest(int instrumentPresence, int responseSpeed) throws InterruptedException,
            ExecutionException, TimeoutException  {
        installPaymentApp(instrumentPresence, responseSpeed);
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);

        // Check the number of instruments.
        assertEquals(
                instrumentPresence == HAVE_INSTRUMENTS ? 3 : 2, getNumberOfPaymentInstruments());

        // Check the labesl of the instruments.
        int i = 0;
        if (instrumentPresence == HAVE_INSTRUMENTS) {
            assertEquals("Bob Pay", getPaymentInstrumentLabel(i++));
        }
        // \u00A0\u22EF is a non-breaking space followed by a midline ellipsis.
        assertEquals("Visa\u00A0\u22EF1111\nJon Doe", getPaymentInstrumentLabel(i++));
        assertEquals("MasterCard\u00A0\u22EF5454\nJon Doe", getPaymentInstrumentLabel(i++));

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
