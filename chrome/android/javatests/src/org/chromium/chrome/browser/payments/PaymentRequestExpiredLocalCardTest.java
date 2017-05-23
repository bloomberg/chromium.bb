// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.Calendar;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a user that pays with an expired local credit card.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
public class PaymentRequestExpiredLocalCardTest implements MainActivityStartCallback {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_free_shipping_test.html", this);

    AutofillTestHelper mHelper;
    String mCreditCardId;

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        mHelper = new AutofillTestHelper();
        // The user has a shipping address on disk.
        String billingAddressId = mHelper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));
        // Create an expired credit card
        mCreditCardId = mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true,
                "Jon Doe", "4111111111111111", "1111", "1", "2016", "visa", R.drawable.visa_card,
                billingAddressId, "" /* serverId */));
    }

    /**
     * Tests that the credit card unmask prompt includes inputs for a new expiration date if the
     * credit card is expired. Also tests that the user can pay once they have entered a new valid
     * expiration date and that merchant receives the updated data.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayWithExpiredCard_ValidExpiration()
            throws InterruptedException, ExecutionException, TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInExpiredCardUnmaskDialogAndWait(
                new int[] {R.id.expiration_month, R.id.expiration_year, R.id.card_unmask_input},
                new String[] {"11", "26", "123"}, mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                DialogInterface.BUTTON_POSITIVE, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"Jon Doe", "4111111111111111",
                "11", "2026", "visa", "123", "Google", "340 Main St", "CA", "Los Angeles", "90291",
                "US", "en", "freeShippingOption"});
    }

    /**
     * Tests that the new expiration date entered in the unmasking prompt for an expired card
     * overwrites that card's old expiration date.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayWithExpiredCard_NewExpirationSaved()
            throws InterruptedException, ExecutionException, TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInExpiredCardUnmaskDialogAndWait(
                new int[] {R.id.expiration_month, R.id.expiration_year, R.id.card_unmask_input},
                new String[] {"11", "26", "123"}, mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                DialogInterface.BUTTON_POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the new expiration date was saved.
        CreditCard storedCard = mHelper.getCreditCard(mCreditCardId);
        Assert.assertEquals("11", storedCard.getMonth());
        Assert.assertEquals("2026", storedCard.getYear());
    }

    /**
     * Tests that it is not possible to add an expired card in a payment request.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCannotAddExpiredCard()
            throws InterruptedException, ExecutionException, TimeoutException {
        // If the current date is in January skip this test. It is not possible to select an expired
        // date in the card editor in January.
        Calendar now = Calendar.getInstance();
        if (now.get(Calendar.MONTH) == 0) return;

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());

        // Try to add an expired card.
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        // Set the expiration date to past month of the current year.
        mPaymentRequestTestRule.setSpinnerSelectionsInCardEditorAndWait(
                new int[] {now.get(Calendar.MONTH) - 1, 0, 0},
                mPaymentRequestTestRule.getBillingAddressChangeProcessed());
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"4111111111111111", "Jon Doe"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInCardEditorAndWait(
                R.id.payments_edit_done_button, mPaymentRequestTestRule.getEditorValidationError());

        // Set the expiration date to the current month of the current year.
        mPaymentRequestTestRule.setSpinnerSelectionsInCardEditorAndWait(
                new int[] {now.get(Calendar.MONTH), 0, 0},
                mPaymentRequestTestRule.getExpirationMonthChange());

        mPaymentRequestTestRule.clickInCardEditorAndWait(
                R.id.payments_edit_done_button, mPaymentRequestTestRule.getReadyToPay());
    }

    /**
     * Tests the different card unmask error messages for an expired card.
     */
    // @MediumTest
    // @Feature({"Payments"})
    // See: crbug.com/687438.
    @Test
    @DisabledTest
    public void testPromptErrorMessages()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Click pay to get to the card unmask prompt.
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());

        // Set valid arguments.
        mPaymentRequestTestRule.setTextInExpiredCardUnmaskDialogAndWait(
                new int[] {R.id.expiration_month, R.id.expiration_year, R.id.card_unmask_input},
                new String[] {"10", "26", "123"},
                mPaymentRequestTestRule.getUnmaskValidationDone());
        Assert.assertTrue(mPaymentRequestTestRule.getUnmaskPromptErrorMessage().equals(""));

        // Set an invalid expiration month.
        mPaymentRequestTestRule.setTextInExpiredCardUnmaskDialogAndWait(
                new int[] {R.id.expiration_month, R.id.expiration_year, R.id.card_unmask_input},
                new String[] {"99", "25", "123"},
                mPaymentRequestTestRule.getUnmaskValidationDone());
        Assert.assertTrue(mPaymentRequestTestRule.getUnmaskPromptErrorMessage().equals(
                "Check your expiration month and try again"));

        // Set an invalid expiration year.
        mPaymentRequestTestRule.setTextInExpiredCardUnmaskDialogAndWait(
                new int[] {R.id.expiration_month, R.id.expiration_year, R.id.card_unmask_input},
                new String[] {"10", "14", "123"},
                mPaymentRequestTestRule.getUnmaskValidationDone());
        Assert.assertTrue(mPaymentRequestTestRule.getUnmaskPromptErrorMessage().equals(
                "Check your expiration year and try again"));

        // If the current date is in January skip this test. It is not possible to select an expired
        // date in January.
        Calendar now = Calendar.getInstance();
        if (now.get(Calendar.MONTH) != 0) {
            String twoDigitsYear = Integer.toString(now.get(Calendar.YEAR)).substring(2);

            // Set an invalid expiration year.
            mPaymentRequestTestRule.setTextInExpiredCardUnmaskDialogAndWait(
                    new int[] {R.id.expiration_month, R.id.expiration_year, R.id.card_unmask_input},
                    new String[] {
                            Integer.toString(now.get(Calendar.MONTH) - 1), twoDigitsYear, "123"},
                    mPaymentRequestTestRule.getUnmaskValidationDone());
            Assert.assertTrue(mPaymentRequestTestRule.getUnmaskPromptErrorMessage().equals(
                    "Check your expiration date and try again"));
        }

        // Set valid arguments again.
        mPaymentRequestTestRule.setTextInExpiredCardUnmaskDialogAndWait(
                new int[] {R.id.expiration_month, R.id.expiration_year, R.id.card_unmask_input},
                new String[] {"10", "26", "123"},
                mPaymentRequestTestRule.getUnmaskValidationDone());
        Assert.assertTrue(mPaymentRequestTestRule.getUnmaskPromptErrorMessage().equals(""));
    }
}
