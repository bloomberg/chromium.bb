// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.support.test.filters.MediumTest;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test to validate the logging of Payment Request metrics.
 */
public class PaymentRequestJourneyLoggerTest extends PaymentRequestTestBase {
    public PaymentRequestJourneyLoggerTest() {
        super("payment_request_metrics_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper mHelper = new AutofillTestHelper();
        // The user has a shipping address and a credit card associated with that address on disk.
        String mBillingAddressId = mHelper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));
        mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                mBillingAddressId, "" /* serverId */));
        // The user also has an incomplete address and an incomplete card saved.
        String mIncompleteAddressId = mHelper.setProfile(new AutofillProfile("",
                "https://example.com", true, "In Complete", "Google", "344 Main St", "CA", "", "",
                "90291", "", "US", "555-555-5555", "", "en-US"));
        mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true, "",
                "4111111111111111", "1111", "18", "2075", "visa", R.drawable.pr_visa,
                mIncompleteAddressId, "" /* serverId */));
    }

    /**
     * Expect that the number of shipping address suggestions was logged properly.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_ShippingAddress_Completed()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Complete a Payment Request with a credit card.
        triggerUIAndWait("ccBuy", mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);

        // Make sure the right number of suggestions were logged.
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.Completed", 2));

        // Make sure no adds, edits or changes were logged.
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.Completed", 0));
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.Completed", 0));
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.Completed", 0));
    }

    /**
     * Expect that the number of shipping address suggestions was logged properly.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_ShippingAddress_AbortedByUser()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Cancel the payment request.
        triggerUIAndWait("ccBuy", mReadyToPay);
        clickAndWait(R.id.close_button, mDismissed);

        // Wait for the histograms to be logged.
        Thread.sleep(200);

        // Make sure the right number of suggestions were logged.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.UserAborted", 2));

        // Make sure no adds, edits or changes were logged.
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.UserAborted", 0));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.UserAborted", 0));
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.UserAborted", 0));
    }

    /**
     * Expect that the NumberOfSelectionEdits histogram gets logged properly for shipping addresses.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSelectionEdits_ShippingAddress_Completed()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Complete a Payment Request with a credit card.
        triggerUIAndWait("ccBuy", mReadyToPay);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);

        // Select the incomplete address and edit it.
        clickOnShippingAddressSuggestionOptionAndWait(1, mReadyToEdit);
        setTextInEditorAndWait(
                new String[] {"In Complete", "Google", "344 Main St", "CA", "Los Angeles"},
                mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);

        // Make sure the edit was logged.
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.Completed", 1));

        // Since the edit was not for the default selection a change should be logged.
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.Completed", 1));

        // Make sure no add was logged.
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.Completed", 0));
    }

    /**
     * Expect that the NumberOfSelectionAdds histogram gets logged properly for shipping addresses.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSelectionAdds_ShippingAddress_Completed()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Complete a Payment Request with a credit card.
        triggerUIAndWait("ccBuy", mReadyToPay);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);

        // Add a new shipping address.
        clickInShippingAddressAndWait(R.id.payments_add_option_button, mReadyToEdit);
        setSpinnerSelectionInEditorAndWait(0 /* Afghanistan */, mReadyToEdit);
        setTextInEditorAndWait(
                new String[] {"Alice", "Supreme Court", "Airport Road", "Kabul", "1043",
                        "999-999-9999"},
                mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);

        // Complete the transaction.
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);

        // Make sure the add was logged.
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.Completed", 1));

        // Make sure no edits or changes were logged.
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.Completed", 0));
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.Completed", 0));
    }

    /**
     * Expect that the number of credit card suggestions was logged properly.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_CreditCards_Completed()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Complete a Payment Request with a credit card.
        triggerUIAndWait("ccBuy", mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);

        // Make sure the right number of suggestions were logged.
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSuggestionsShown.CreditCards.Completed", 2));

        // Make sure no adds, edits or changes were logged.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.NumberOfSelectionAdds.CreditCards.Completed", 0));
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionChanges.CreditCards.Completed", 0));
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.NumberOfSelectionEdits.CreditCards.Completed", 0));
    }

    /**
     * Expect that the number of credit card suggestions was logged properly.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_CreditCards_AbortedByUser()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Cancel the payment request.
        triggerUIAndWait("ccBuy", mReadyToPay);
        clickAndWait(R.id.close_button, mDismissed);

        // Wait for the histograms to be logged.
        Thread.sleep(200);

        // Make sure the right number of suggestions were logged.
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSuggestionsShown.CreditCards.UserAborted", 2));

        // Make sure no adds, edits or changes were logged.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.NumberOfSelectionAdds.CreditCards.UserAborted", 0));
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionChanges.CreditCards.UserAborted", 0));
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionEdits.CreditCards.UserAborted", 0));
    }

    /**
     * Expect that the NumberOfSelectionAdds histogram gets logged properly for credit cards.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSelectionAdds_CreditCards_Completed()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Complete a Payment Request with a credit card.
        triggerUIAndWait("ccBuy", mReadyToPay);

        // Add a new credit card.
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, mReadyToEdit);
        setSpinnerSelectionsInCardEditorAndWait(
                new int[] {11, 1, 0}, mBillingAddressChangeProcessed);
        setTextInCardEditorAndWait(new String[] {"4111111111111111", "Jon Doe"}, mEditorTextUpdate);
        clickInCardEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);

        // Complete the transaction.
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);

        // Make sure the add was logged.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.NumberOfSelectionAdds.CreditCards.Completed", 1));

        // Make sure no edits or changes were logged.
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionChanges.CreditCards.Completed", 0));
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.NumberOfSelectionEdits.CreditCards.Completed", 0));
    }

    /**
     * Expect that no metric for contact info has been logged.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNoContactInfoHistogram()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Complete a Payment Request with a credit card.
        triggerUIAndWait("ccBuy", mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);

        // Make sure nothing was logged for contact info.
        assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSuggestionsShown.ContactInfo.Completed", 2));
        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.NumberOfSelectionAdds.ContactInfo.Completed", 0));
        assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionChanges.ContactInfo.Completed", 0));
        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.NumberOfSelectionEdits.ContactInfo.Completed", 0));
    }

    /**
     * Expect that that the journey metrics are logged correctly on a second consecutive payment
     * request.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testTwoTimes() throws InterruptedException, ExecutionException, TimeoutException {
        // Complete a Payment Request with a credit card.
        triggerUIAndWait("ccBuy", mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);

        // Make sure the right number of suggestions were logged.
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.Completed", 2));

        // Make sure no adds, edits or changes were logged.
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.Completed", 0));
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.Completed", 0));
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.Completed", 0));

        // Complete a second Payment Request with a credit card.
        reTriggerUIAndWait("ccBuy", mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);

        // Make sure the right number of suggestions were logged.
        assertEquals(
                2, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.Completed", 2));

        // Make sure no adds, edits or changes were logged.
        assertEquals(
                2, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.Completed", 0));
        assertEquals(
                2, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.Completed", 0));
        assertEquals(
                2, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.Completed", 0));
    }

    /**
     * Expect that no journey metrics are logged if the payment request was not shown to the user.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNoShow() throws InterruptedException, ExecutionException, TimeoutException {
        // Android Pay is supported but no instruments are present.
        installPaymentApp("https://android.com/pay", NO_INSTRUMENTS, DELAYED_RESPONSE);
        openPageAndClickNodeAndWait("androidPayBuy", mShowFailed);
        expectResultContains(new String[] {"The payment method is not supported"});

        // Make sure that no journey metrics were logged.
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.UserAborted", 2));
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.OtherAborted", 2));
        assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.Completed", 2));
    }
}
