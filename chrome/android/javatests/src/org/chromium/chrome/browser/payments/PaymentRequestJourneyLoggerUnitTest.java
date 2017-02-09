// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.SmallTest;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.util.ApplicationData;
import org.chromium.content.browser.test.NativeLibraryTestBase;

/**
 * Tests for the PaymentRequestJourneyLogger class.
 */
public class PaymentRequestJourneyLoggerUnitTest extends NativeLibraryTestBase {
    @Override
    public void setUp() throws Exception {
        super.setUp();
        ApplicationData.clearAppData(getInstrumentation().getTargetContext());
        loadNativeLibraryAndInitBrowserProcess();
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant does not use it and does not
     * show the PaymentRequest to the user.
     */
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentNotCalled_NoShow() {
        PaymentRequestJourneyLogger logger = new PaymentRequestJourneyLogger();
        assertNoLogForCanMakePayment();

        logger.recordJourneyStatsHistograms("");

        // CanMakePayment was not used.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.Usage",
                                PaymentRequestJourneyLogger.CAN_MAKE_PAYMENT_NOT_USED));

        // There should be no completion stats since PR was not shown to the user
        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                                PaymentRequestJourneyLogger.COMPLETION_STATUS_ABORTED));
        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                                PaymentRequestJourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant does not use it and the
     * transaction is aborted.
     */
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentNotCalled_ShowAndAbort() {
        PaymentRequestJourneyLogger logger = new PaymentRequestJourneyLogger();
        assertNoLogForCanMakePayment();

        // The merchant does not query CanMakePayment, show the PaymentRequest and the user
        // aborts it.
        logger.setShowCalled();
        logger.recordJourneyStatsHistograms("Aborted");

        // CanMakePayment was not used.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.Usage",
                                PaymentRequestJourneyLogger.CAN_MAKE_PAYMENT_NOT_USED));

        // There should be a record for an abort when CanMakePayment is not used but the PR is shown
        // to the user.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                                PaymentRequestJourneyLogger.COMPLETION_STATUS_ABORTED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant does not use it and the
     * transaction is completed.
     */
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentNotCalled_ShowAndComplete() {
        PaymentRequestJourneyLogger logger = new PaymentRequestJourneyLogger();
        assertNoLogForCanMakePayment();

        // The merchant does not query CanMakePayment, show the PaymentRequest and the user
        // completes it.
        logger.setShowCalled();
        logger.recordJourneyStatsHistograms("Completed");

        // CanMakePayment was not used.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.Usage",
                                PaymentRequestJourneyLogger.CAN_MAKE_PAYMENT_NOT_USED));

        // There should be a record for a completion when CanMakePayment is not used but the PR is
        // shown to the user.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                                PaymentRequestJourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant uses it, returns false and
     * show is not called.
     */
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentCalled_FalseAndNoShow() {
        PaymentRequestJourneyLogger logger = new PaymentRequestJourneyLogger();
        assertNoLogForCanMakePayment();

        // The user cannot make payment and the PaymentRequest is not shown.
        logger.setCanMakePaymentValue(false);
        logger.recordJourneyStatsHistograms("");

        // CanMakePayment was used.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.Usage",
                                PaymentRequestJourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being false and not shown.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.Used.EffetOnShow",
                                PaymentRequestJourneyLogger
                                        .CMP_SHOW_COULD_NOT_MAKE_PAYMENT_AND_DID_NOT_SHOW));

        // There should be no completion stats since PR was not shown to the user.
        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                                PaymentRequestJourneyLogger.COMPLETION_STATUS_ABORTED));
        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                                PaymentRequestJourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant uses it, returns true and
     * show is not called.
     */
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentCalled_TrueAndNoShow() {
        PaymentRequestJourneyLogger logger = new PaymentRequestJourneyLogger();
        assertNoLogForCanMakePayment();

        // The user can make a payment but the Payment Request is not shown.
        logger.setCanMakePaymentValue(true);
        logger.recordJourneyStatsHistograms("");

        // CanMakePayment was used.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.Usage",
                                PaymentRequestJourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being true and not shown.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.Used.EffetOnShow",
                                PaymentRequestJourneyLogger.CMP_SHOW_COULD_MAKE_PAYMENT));

        // There should be no completion stats since PR was not shown to the user.
        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                                PaymentRequestJourneyLogger.COMPLETION_STATUS_ABORTED));
        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                                PaymentRequestJourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant uses it, returns false, show
     * is called but the transaction is aborted.
     */
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentCalled_FalseShowAndAbort() {
        PaymentRequestJourneyLogger logger = new PaymentRequestJourneyLogger();
        assertNoLogForCanMakePayment();

        // The user cannot make a payment. the payment request is shown but aborted.
        logger.setShowCalled();
        logger.setCanMakePaymentValue(false);
        logger.recordJourneyStatsHistograms("Aborted");

        // CanMakePayment was used.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.Usage",
                                PaymentRequestJourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being true and not shown.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.Used.EffetOnShow",
                                PaymentRequestJourneyLogger.CMP_SHOW_DID_SHOW));

        // There should be a record for an abort when CanMakePayment is false but the PR is shown to
        // the user.
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
                           PaymentRequestJourneyLogger.COMPLETION_STATUS_ABORTED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant uses it, returns false,
     * show is called and the transaction is completed.
     */
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentCalled_FalseShowAndComplete() {
        PaymentRequestJourneyLogger logger = new PaymentRequestJourneyLogger();
        assertNoLogForCanMakePayment();

        // The user cannot make a payment. the payment request is shown and completed.
        logger.setShowCalled();
        logger.setCanMakePaymentValue(false);
        logger.recordJourneyStatsHistograms("Completed");

        // CanMakePayment was used.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.Usage",
                                PaymentRequestJourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being true and not shown.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.Used.EffetOnShow",
                                PaymentRequestJourneyLogger.CMP_SHOW_DID_SHOW));

        // There should be a record for a completion when CanMakePayment is false and the PR is
        // shown to the user.
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(
                           "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
                           PaymentRequestJourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant uses it, returns true, show
     * is called but the transaction is aborted.
     */
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentCalled_TrueShowAndAbort() {
        PaymentRequestJourneyLogger logger = new PaymentRequestJourneyLogger();
        assertNoLogForCanMakePayment();

        // The user cannot make a payment. the payment request is shown and completed.
        logger.setShowCalled();
        logger.setCanMakePaymentValue(true);
        logger.recordJourneyStatsHistograms("Aborted");

        // CanMakePayment was used.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.Usage",
                                PaymentRequestJourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being true and not shown.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.Used.EffetOnShow",
                                PaymentRequestJourneyLogger.CMP_SHOW_DID_SHOW
                                        | PaymentRequestJourneyLogger.CMP_SHOW_COULD_MAKE_PAYMENT));

        // There should be a record for an abort when CanMakePayment is true and the PR is shown to
        // the user.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.Used.TrueWithShowEffectOnCompletion",
                                PaymentRequestJourneyLogger.COMPLETION_STATUS_ABORTED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant uses it, returns true, show
     * is called and the transaction is completed.
     */
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentCalled_TrueShowAndComplete() {
        PaymentRequestJourneyLogger logger = new PaymentRequestJourneyLogger();
        assertNoLogForCanMakePayment();

        // The user cannot make a payment. the payment request is shown and completed.
        logger.setShowCalled();
        logger.setCanMakePaymentValue(true);
        logger.recordJourneyStatsHistograms("Completed");

        // CanMakePayment was used.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.Usage",
                                PaymentRequestJourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being true and not shown.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.Used.EffetOnShow",
                                PaymentRequestJourneyLogger.CMP_SHOW_DID_SHOW
                                        | PaymentRequestJourneyLogger.CMP_SHOW_COULD_MAKE_PAYMENT));

        // There should be a record for a completion when CanMakePayment is true and the PR is shown
        // to the user.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.CanMakePayment.Used.TrueWithShowEffectOnCompletion",
                                PaymentRequestJourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests that the completion status metrics based on whether the user had suggestions for all
     * the requested sections are logged as correctly.
     */
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_SuggestionsForEverything_Completed() {
        PaymentRequestJourneyLogger logger = new PaymentRequestJourneyLogger();

        // Simulate that the user had suggestions for all the requested sections.
        logger.setNumberOfSuggestionsShown(PaymentRequestJourneyLogger.SECTION_CREDIT_CARDS, 1);

        // Simulate that the user completes the checkout.
        logger.recordJourneyStatsHistograms("Completed");

        // Make sure the appropriate metric was logged.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.UserHadSuggestionsForEverything.EffectOnCompletion",
                                PaymentRequestJourneyLogger.COMPLETION_STATUS_COMPLETED));

        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserDidNotHaveSuggestionsForEverything.EffectOnCompletion",
                        PaymentRequestJourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests that the completion status metrics based on whether the user had suggestions for all
     * the requested sections are logged as correctly.
     */
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_SuggestionsForEverything_Aborted() {
        PaymentRequestJourneyLogger logger = new PaymentRequestJourneyLogger();

        // Simulate that the user had suggestions for all the requested sections.
        logger.setNumberOfSuggestionsShown(PaymentRequestJourneyLogger.SECTION_CREDIT_CARDS, 1);

        // Simulate that the user aborts the checkout.
        logger.recordJourneyStatsHistograms("Aborted");

        // Make sure the appropriate metric was logged.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.UserHadSuggestionsForEverything.EffectOnCompletion",
                                PaymentRequestJourneyLogger.COMPLETION_STATUS_ABORTED));

        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserDidNotHaveSuggestionsForEverything.EffectOnCompletion",
                        PaymentRequestJourneyLogger.COMPLETION_STATUS_ABORTED));
    }

    /**
     * Tests that the completion status metrics based on whether the user had suggestions for all
     * the requested sections are logged as correctly.
     */
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_NoSuggestionsForEverything_Completed() {
        PaymentRequestJourneyLogger logger = new PaymentRequestJourneyLogger();

        // Simulate that the user did not have suggestions for all the requested sections.
        logger.setNumberOfSuggestionsShown(PaymentRequestJourneyLogger.SECTION_CREDIT_CARDS, 0);

        // Simulate that the user completes the checkout.
        logger.recordJourneyStatsHistograms("Completed");

        // Make sure the appropriate metric was logged.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserDidNotHaveSuggestionsForEverything.EffectOnCompletion",
                        PaymentRequestJourneyLogger.COMPLETION_STATUS_COMPLETED));

        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.UserHadSuggestionsForEverything.EffectOnCompletion",
                                PaymentRequestJourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests that the completion status metrics based on whether the user had suggestions for all
     * the requested sections are logged as correctly.
     */
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_NoSuggestionsForEverything_Aborted() {
        PaymentRequestJourneyLogger logger = new PaymentRequestJourneyLogger();

        // Simulate that the user had suggestions for all the requested sections.
        logger.setNumberOfSuggestionsShown(PaymentRequestJourneyLogger.SECTION_CREDIT_CARDS, 0);

        // Simulate that the user aborts the checkout.
        logger.recordJourneyStatsHistograms("Aborted");

        // Make sure the appropriate metric was logged.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserDidNotHaveSuggestionsForEverything.EffectOnCompletion",
                        PaymentRequestJourneyLogger.COMPLETION_STATUS_ABORTED));

        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                                "PaymentRequest.UserHadSuggestionsForEverything.EffectOnCompletion",
                                PaymentRequestJourneyLogger.COMPLETION_STATUS_ABORTED));
    }

    /**
     * Asserts that no histogram is logged.
     */
    private void assertNoLogForCanMakePayment() {
        // Use stats.
        for (int i = 0; i < PaymentRequestJourneyLogger.CAN_MAKE_PAYMENT_USE_MAX; ++i) {
            assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                                    "PaymentRequest.CanMakePayment.Usage", i));
        }

        // Effect on show stats.
        for (int i = 0; i < PaymentRequestJourneyLogger.CMP_SHOW_MAX; ++i) {
            assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                                    "PaymentRequest.CanMakePayment.Used.EffetOnShow", i));
        }

        // Effect on completion stats.
        for (int i = 0; i < PaymentRequestJourneyLogger.COMPLETION_STATUS_MAX; ++i) {
            assertEquals(0,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion", i));
            assertEquals(
                    0, RecordHistogram.getHistogramValueCountForTesting(
                               "PaymentRequest.CanMakePayment.Used.TrueWithShowEffectOnCompletion",
                               i));
            assertEquals(
                    0, RecordHistogram.getHistogramValueCountForTesting(
                               "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
                               i));
        }
    }
}