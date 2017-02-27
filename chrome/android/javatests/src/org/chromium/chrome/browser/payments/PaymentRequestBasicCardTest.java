// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for "basic-card" payment method.
 */
@CommandLineFlags.Add("enable-blink-features=PaymentRequestBasicCard")
public class PaymentRequestBasicCardTest extends PaymentRequestTestBase {
    public PaymentRequestBasicCardTest() {
        super("payment_request_basic_card_test.html");
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
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                billingAddressId, "" /* serverId */));
    }

    @MediumTest
    @Feature({"Payments"})
    public void testCanPayWithBasicCard() throws InterruptedException,
            ExecutionException, TimeoutException {
        openPageAndClickNodeAndWait("checkBasicCard", mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"true"});

        clickNodeAndWait("buyBasicCard", mReadyForInput);
    }

    @MediumTest
    @Feature({"Payments"})
    public void testIgnoreCardType() throws InterruptedException,
            ExecutionException, TimeoutException {
        openPageAndClickNodeAndWait("checkBasicDebit", mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"true"});

        clickNodeAndWait("buyBasicDebit", mReadyForInput);
    }

    @MediumTest
    @Feature({"Payments"})
    public void testCannotMakeActivePaymentWithBasicMasterCard() throws InterruptedException,
            ExecutionException, TimeoutException {
        openPageAndClickNodeAndWait("checkBasicMasterCard", mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"false"});

        reTriggerUIAndWait("buyBasicMasterCard", mReadyForInput);
    }

    @MediumTest
    @Feature({"Payments"})
    public void testSupportedNetworksMustMatchForCanMakePayment()
            throws InterruptedException, ExecutionException, TimeoutException {
        openPageAndClickNodeAndWait("checkBasicVisa", mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"true"});

        clickNodeAndWait("checkBasicMasterCard", mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"Query quota exceeded"});

        clickNodeAndWait("checkBasicVisa", mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"true"});
    }

    @MediumTest
    @Feature({"Payments"})
    public void testSupportedTypesMustMatchForCanMakePayment()
            throws InterruptedException, ExecutionException, TimeoutException {
        openPageAndClickNodeAndWait("checkBasicVisa", mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"true"});

        clickNodeAndWait("checkBasicDebit", mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"Query quota exceeded"});

        clickNodeAndWait("checkBasicVisa", mCanMakePaymentQueryResponded);
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
        openPageAndClickNodeAndWait("checkBasicVisa", mCanMakePaymentQueryResponded);
        expectResultContains(new String[] {"true"});

        reTriggerUIAndWait("buy", mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
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
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, mReadyToEdit);
        setTextInCardEditorAndWait(new String[] {"5555-5555-5555-4444", "Jane Jones"},
                mEditorTextUpdate);
        setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS},
                mBillingAddressChangeProcessed);
        clickInCardEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
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
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, mReadyToEdit);
        setTextInCardEditorAndWait(new String[] {"4242-4242-4242-4242", "Jane Jones"},
                mEditorTextUpdate);
        setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS},
                mBillingAddressChangeProcessed);
        clickInCardEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(
                new String[] {"4242424242424242", "12", "Jane Jones", "123", "basic-card"});
    }
}
