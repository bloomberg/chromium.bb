// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.DELAYED_RESPONSE;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.NO_INSTRUMENTS;

import android.content.DialogInterface;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test to validate the logging of Payment Request metrics.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
public class PaymentRequestJourneyLoggerTest implements MainActivityStartCallback {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_metrics_test.html", this);

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper mHelper = new AutofillTestHelper();
        // The user has a shipping address and a credit card associated with that address on disk.
        String mBillingAddressId = mHelper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "650-253-0000", "", "en-US"));
        mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                mBillingAddressId, "" /* serverId */));
        // The user also has an incomplete address and an incomplete card saved.
        String mIncompleteAddressId = mHelper.setProfile(new AutofillProfile("",
                "https://example.com", true, "In Complete", "Google", "344 Main St", "CA", "", "",
                "90291", "", "US", "650-253-0000", "", "en-US"));
        mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true, "",
                "4111111111111111", "1111", "18", "2075", "visa", R.drawable.visa_card,
                mIncompleteAddressId, "" /* serverId */));
    }

    /**
     * Expect that the number of shipping address suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_ShippingAddress_Completed()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                DialogInterface.BUTTON_POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.Completed", 2));

        // Make sure no adds, edits or changes were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.Completed", 0));
    }

    /**
     * Expect that the number of shipping address suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_ShippingAddress_AbortedByUser()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());

        // Wait for the histograms to be logged.
        Thread.sleep(200);

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.UserAborted", 2));

        // Make sure no adds, edits or changes were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.UserAborted", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.UserAborted", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.UserAborted", 0));
    }

    /**
     * Expect that the NumberOfSelectionEdits histogram gets logged properly for shipping addresses.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSelectionEdits_ShippingAddress_Completed()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Select the incomplete address and edit it.
        mPaymentRequestTestRule.clickOnShippingAddressSuggestionOptionAndWait(
                1, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"In Complete", "Google", "344 Main St", "CA", "Los Angeles"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_done_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                DialogInterface.BUTTON_POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the edit was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.Completed", 1));

        // Since the edit was not for the default selection a change should be logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.Completed", 1));

        // Make sure no add was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.Completed", 0));
    }

    /**
     * Expect that the NumberOfSelectionAdds histogram gets logged properly for shipping addresses.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSelectionAdds_ShippingAddress_Completed()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Add a new shipping address.
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setSpinnerSelectionInEditorAndWait(
                0 /* Afghanistan */, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {
                        "Alice", "Supreme Court", "Airport Road", "Kabul", "1043", "020-253-0000"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_done_button, mPaymentRequestTestRule.getReadyToPay());

        // Complete the transaction.
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                DialogInterface.BUTTON_POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the add was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.Completed", 1));

        // Make sure no edits or changes were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.Completed", 0));
    }

    /**
     * Expect that the number of credit card suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_CreditCards_Completed()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                DialogInterface.BUTTON_POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.CreditCards.Completed", 2));

        // Make sure no adds, edits or changes were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.CreditCards.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.CreditCards.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.CreditCards.Completed", 0));
    }

    /**
     * Expect that the number of credit card suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_CreditCards_AbortedByUser()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());

        // Wait for the histograms to be logged.
        Thread.sleep(200);

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.CreditCards.UserAborted", 2));

        // Make sure no adds, edits or changes were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.CreditCards.UserAborted", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.CreditCards.UserAborted", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.CreditCards.UserAborted", 0));
    }

    /**
     * Expect that the NumberOfSelectionAdds histogram gets logged properly for credit cards.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSelectionAdds_CreditCards_Completed()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());

        // Add a new credit card.
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setSpinnerSelectionsInCardEditorAndWait(
                new int[] {11, 1, 0}, mPaymentRequestTestRule.getBillingAddressChangeProcessed());
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"4111111111111111", "Jon Doe"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInCardEditorAndWait(
                R.id.payments_edit_done_button, mPaymentRequestTestRule.getReadyToPay());

        // Complete the transaction.
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                DialogInterface.BUTTON_POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the add was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.CreditCards.Completed", 1));

        // Make sure no edits or changes were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.CreditCards.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.CreditCards.Completed", 0));
    }

    /**
     * Expect that no metric for contact info has been logged.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoContactInfoHistogram()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                DialogInterface.BUTTON_POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure nothing was logged for contact info.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ContactInfo.Completed", 2));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ContactInfo.Completed", 0));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ContactInfo.Completed", 0));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ContactInfo.Completed", 0));
    }

    /**
     * Expect that that the journey metrics are logged correctly on a second consecutive payment
     * request.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testTwoTimes() throws InterruptedException, ExecutionException, TimeoutException {
        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                DialogInterface.BUTTON_POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.Completed", 2));

        // Make sure no adds, edits or changes were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.Completed", 0));

        // Complete a second Payment Request with a credit card.
        mPaymentRequestTestRule.reTriggerUIAndWait(
                "ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                DialogInterface.BUTTON_POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.Completed", 2));

        // Make sure no adds, edits or changes were logged.
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.Completed", 0));
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.Completed", 0));
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.Completed", 0));
    }

    /**
     * Expect that no journey metrics are logged if the payment request was not shown to the user.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoShow() throws InterruptedException, ExecutionException, TimeoutException {
        // Android Pay is supported but no instruments are present.
        mPaymentRequestTestRule.installPaymentApp(
                "https://android.com/pay", NO_INSTRUMENTS, DELAYED_RESPONSE);
        mPaymentRequestTestRule.openPageAndClickNodeAndWait(
                "androidPayBuy", mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"The payment method is not supported"});

        // Make sure that no journey metrics were logged.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.UserAborted", 2));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.OtherAborted", 2));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.Completed", 2));
    }
}
