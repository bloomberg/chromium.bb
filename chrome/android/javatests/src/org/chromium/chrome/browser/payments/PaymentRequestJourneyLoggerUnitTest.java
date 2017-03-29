// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.util.ApplicationData;
import org.chromium.components.payments.JourneyLogger;
import org.chromium.content.browser.test.NativeLibraryTestRule;

/**
 * Tests for the PaymentRequestJourneyLogger class.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PaymentRequestJourneyLoggerUnitTest {
    @Rule
    public NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    @Before
    public void setUp() throws Exception {
        ApplicationData.clearAppData(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        mNativeLibraryTestRule.loadNativeLibraryAndInitBrowserProcess();
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant does not use it and does not
     * show the PaymentRequest to the user.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentNotCalled_NoShow() {
        JourneyLogger logger = new JourneyLogger();
        assertNoLogForCanMakePayment();

        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_USER_ABORTED);

        // CanMakePayment was not used.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_NOT_USED));

        // There should be no completion stats since PR was not shown to the user
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_USER_ABORTED));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant does not use it and the
     * transaction is aborted.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentNotCalled_ShowAndUserAbort() {
        JourneyLogger logger = new JourneyLogger();
        assertNoLogForCanMakePayment();

        // The merchant does not query CanMakePayment, show the PaymentRequest and the user
        // aborts it.
        logger.setShowCalled();
        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_USER_ABORTED);

        // CanMakePayment was not used.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_NOT_USED));

        // There should be a record for an abort when CanMakePayment is not used but the PR is shown
        // to the user.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_USER_ABORTED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant does not use it and the
     * transaction is aborted.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentNotCalled_ShowAndOtherAbort() {
        JourneyLogger logger = new JourneyLogger();
        assertNoLogForCanMakePayment();

        // The merchant does not query CanMakePayment, show the PaymentRequest and the user
        // aborts it.
        logger.setShowCalled();
        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_OTHER_ABORTED);

        // CanMakePayment was not used.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_NOT_USED));

        // There should be a record for an abort when CanMakePayment is not used but the PR is shown
        // to the user.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_OTHER_ABORTED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant does not use it and the
     * transaction is completed.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentNotCalled_ShowAndComplete() {
        JourneyLogger logger = new JourneyLogger();
        assertNoLogForCanMakePayment();

        // The merchant does not query CanMakePayment, show the PaymentRequest and the user
        // completes it.
        logger.setShowCalled();
        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_COMPLETED);

        // CanMakePayment was not used.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_NOT_USED));

        // There should be a record for a completion when CanMakePayment is not used but the PR is
        // shown to the user.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant uses it, returns false and
     * show is not called.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentCalled_FalseAndNoShow() {
        JourneyLogger logger = new JourneyLogger();
        assertNoLogForCanMakePayment();

        // The user cannot make payment and the PaymentRequest is not shown.
        logger.setCanMakePaymentValue(false);
        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_OTHER_ABORTED);

        // CanMakePayment was used.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being false and not shown.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.EffectOnShow",
                        JourneyLogger.CMP_SHOW_COULD_NOT_MAKE_PAYMENT_AND_DID_NOT_SHOW));

        // There should be no completion stats since PR was not shown to the user.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_OTHER_ABORTED));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant uses it, returns true and
     * show is not called.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentCalled_TrueAndNoShow() {
        JourneyLogger logger = new JourneyLogger();
        assertNoLogForCanMakePayment();

        // The user can make a payment but the Payment Request is not shown.
        logger.setCanMakePaymentValue(true);
        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_OTHER_ABORTED);

        // CanMakePayment was used.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being true and not shown.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.EffectOnShow",
                        JourneyLogger.CMP_SHOW_COULD_MAKE_PAYMENT));

        // There should be no completion stats since PR was not shown to the user.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_OTHER_ABORTED));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant uses it, returns false, show
     * is called but the transaction is aborted by the user.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentCalled_FalseShowAndUserAbort() {
        JourneyLogger logger = new JourneyLogger();
        assertNoLogForCanMakePayment();

        // The user cannot make a payment. the payment request is shown but aborted.
        logger.setShowCalled();
        logger.setCanMakePaymentValue(false);
        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_USER_ABORTED);

        // CanMakePayment was used.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being true and not shown.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.EffectOnShow",
                        JourneyLogger.CMP_SHOW_DID_SHOW));

        // There should be a record for an abort when CanMakePayment is false but the PR is shown to
        // the user.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_USER_ABORTED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant uses it, returns false, show
     * is called but the transaction is aborted.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentCalled_FalseShowAndOtherAbort() {
        JourneyLogger logger = new JourneyLogger();
        assertNoLogForCanMakePayment();

        // The user cannot make a payment. the payment request is shown but aborted.
        logger.setShowCalled();
        logger.setCanMakePaymentValue(false);
        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_OTHER_ABORTED);

        // CanMakePayment was used.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being true and not shown.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.EffectOnShow",
                        JourneyLogger.CMP_SHOW_DID_SHOW));

        // There should be a record for an abort when CanMakePayment is false but the PR is shown to
        // the user.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_OTHER_ABORTED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant uses it, returns false,
     * show is called and the transaction is completed.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentCalled_FalseShowAndComplete() {
        JourneyLogger logger = new JourneyLogger();
        assertNoLogForCanMakePayment();

        // The user cannot make a payment. the payment request is shown and completed.
        logger.setShowCalled();
        logger.setCanMakePaymentValue(false);
        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_COMPLETED);

        // CanMakePayment was used.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being true and not shown.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.EffectOnShow",
                        JourneyLogger.CMP_SHOW_DID_SHOW));

        // There should be a record for a completion when CanMakePayment is false and the PR is
        // shown to the user.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant uses it, returns true, show
     * is called but the transaction is aborted by the user.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentCalled_TrueShowAndUserAbort() {
        JourneyLogger logger = new JourneyLogger();
        assertNoLogForCanMakePayment();

        // The user cannot make a payment. the payment request is shown and completed.
        logger.setShowCalled();
        logger.setCanMakePaymentValue(true);
        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_USER_ABORTED);

        // CanMakePayment was used.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being true and not shown.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.EffectOnShow",
                        JourneyLogger.CMP_SHOW_DID_SHOW
                                | JourneyLogger.CMP_SHOW_COULD_MAKE_PAYMENT));

        // There should be a record for an abort when CanMakePayment is true and the PR is shown to
        // the user.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.TrueWithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_USER_ABORTED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant uses it, returns true, show
     * is called but the transaction is aborted.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentCalled_TrueShowAndOtherAbort() {
        JourneyLogger logger = new JourneyLogger();
        assertNoLogForCanMakePayment();

        // The user cannot make a payment. the payment request is shown and completed.
        logger.setShowCalled();
        logger.setCanMakePaymentValue(true);
        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_OTHER_ABORTED);

        // CanMakePayment was used.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being true and not shown.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.EffectOnShow",
                        JourneyLogger.CMP_SHOW_DID_SHOW
                                | JourneyLogger.CMP_SHOW_COULD_MAKE_PAYMENT));

        // There should be a record for an abort when CanMakePayment is true and the PR is shown to
        // the user.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.TrueWithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_OTHER_ABORTED));
    }

    /**
     * Tests the canMakePayment stats for the case where the merchant uses it, returns true, show
     * is called and the transaction is completed.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_CanMakePaymentCalled_TrueShowAndComplete() {
        JourneyLogger logger = new JourneyLogger();
        assertNoLogForCanMakePayment();

        // The user cannot make a payment. the payment request is shown and completed.
        logger.setShowCalled();
        logger.setCanMakePaymentValue(true);
        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_COMPLETED);

        // CanMakePayment was used.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being true and not shown.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.EffectOnShow",
                        JourneyLogger.CMP_SHOW_DID_SHOW
                                | JourneyLogger.CMP_SHOW_COULD_MAKE_PAYMENT));

        // There should be a record for a completion when CanMakePayment is true and the PR is shown
        // to the user.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.TrueWithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests that the completion status metrics based on whether the user had suggestions for all
     * the requested sections are logged as correctly.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_SuggestionsForEverything_Completed() {
        JourneyLogger logger = new JourneyLogger();

        // Simulate that the user had suggestions for all the requested sections.
        logger.setNumberOfSuggestionsShown(JourneyLogger.SECTION_CREDIT_CARDS, 1);

        // Simulate that the user completes the checkout.
        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_COMPLETED);

        // Make sure the appropriate metric was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserHadSuggestionsForEverything.EffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_COMPLETED));

        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserDidNotHaveSuggestionsForEverything.EffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests that the completion status metrics based on whether the user had suggestions for all
     * the requested sections are logged as correctly.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_SuggestionsForEverything_UserAborted() {
        JourneyLogger logger = new JourneyLogger();

        // Simulate that the user had suggestions for all the requested sections.
        logger.setNumberOfSuggestionsShown(JourneyLogger.SECTION_CREDIT_CARDS, 1);

        // Simulate that the user aborts the checkout.
        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_USER_ABORTED);

        // Make sure the appropriate metric was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserHadSuggestionsForEverything.EffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_USER_ABORTED));

        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserDidNotHaveSuggestionsForEverything.EffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_USER_ABORTED));
    }

    /**
     * Tests that the completion status metrics based on whether the user had suggestions for all
     * the requested sections are logged as correctly.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_SuggestionsForEverything_OtherAborted() {
        JourneyLogger logger = new JourneyLogger();

        // Simulate that the user had suggestions for all the requested sections.
        logger.setNumberOfSuggestionsShown(JourneyLogger.SECTION_CREDIT_CARDS, 1);

        // Simulate that the user aborts the checkout.
        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_OTHER_ABORTED);

        // Make sure the appropriate metric was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserHadSuggestionsForEverything.EffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_OTHER_ABORTED));

        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserDidNotHaveSuggestionsForEverything.EffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_OTHER_ABORTED));
    }

    /**
     * Tests that the completion status metrics based on whether the user had suggestions for all
     * the requested sections are logged as correctly.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_NoSuggestionsForEverything_Completed() {
        JourneyLogger logger = new JourneyLogger();

        // Simulate that the user did not have suggestions for all the requested sections.
        logger.setNumberOfSuggestionsShown(JourneyLogger.SECTION_CREDIT_CARDS, 0);

        // Simulate that the user completes the checkout.
        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_COMPLETED);

        // Make sure the appropriate metric was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserDidNotHaveSuggestionsForEverything.EffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_COMPLETED));

        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserHadSuggestionsForEverything.EffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests that the completion status metrics based on whether the user had suggestions for all
     * the requested sections are logged as correctly.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_NoSuggestionsForEverything_UserAborted() {
        JourneyLogger logger = new JourneyLogger();

        // Simulate that the user did not have suggestions for all the requested sections.
        logger.setNumberOfSuggestionsShown(JourneyLogger.SECTION_CREDIT_CARDS, 0);

        // Simulate that the user aborts the checkout.
        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_USER_ABORTED);

        // Make sure the appropriate metric was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserDidNotHaveSuggestionsForEverything.EffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_USER_ABORTED));

        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserHadSuggestionsForEverything.EffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_USER_ABORTED));
    }

    /**
     * Tests that the completion status metrics based on whether the user had suggestions for all
     * the requested sections are logged as correctly.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_NoSuggestionsForEverything_OtherAborted() {
        JourneyLogger logger = new JourneyLogger();

        // Simulate that the user did not have suggestions for all the requested sections.
        logger.setNumberOfSuggestionsShown(JourneyLogger.SECTION_CREDIT_CARDS, 0);

        // Simulate that the user aborts the checkout.
        logger.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_OTHER_ABORTED);

        // Make sure the appropriate metric was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserDidNotHaveSuggestionsForEverything.EffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_OTHER_ABORTED));

        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserHadSuggestionsForEverything.EffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_OTHER_ABORTED));
    }

    /**
     * Tests that the metrics are logged correctly for two simultaneous Payment Requests.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRecordJourneyStatsHistograms_TwoPaymentRequests() {
        JourneyLogger logger1 = new JourneyLogger();
        JourneyLogger logger2 = new JourneyLogger();

        // Make the two loggers have different data.
        logger1.setShowCalled();
        logger2.setShowCalled();

        logger1.setCanMakePaymentValue(true);

        logger1.setNumberOfSuggestionsShown(JourneyLogger.SECTION_CREDIT_CARDS, 1);
        logger2.setNumberOfSuggestionsShown(JourneyLogger.SECTION_CREDIT_CARDS, 0);

        // Simulate that the user completes one checkout and aborts the other.
        logger1.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_COMPLETED);
        logger2.recordJourneyStatsHistograms(JourneyLogger.COMPLETION_STATUS_USER_ABORTED);

        // Make sure the appropriate metric was logged for logger1.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserHadSuggestionsForEverything.EffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_COMPLETED));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_USED));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.TrueWithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_COMPLETED));

        // Make sure the appropriate metric was logged for logger2.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.UserDidNotHaveSuggestionsForEverything.EffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_USER_ABORTED));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_NOT_USED));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_USER_ABORTED));
    }

    /**
     * Asserts that no histogram is logged.
     */
    private void assertNoLogForCanMakePayment() {
        // Use stats.
        for (int i = 0; i < JourneyLogger.CAN_MAKE_PAYMENT_USE_MAX; ++i) {
            Assert.assertEquals(0,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "PaymentRequest.CanMakePayment.Usage", i));
        }

        // Effect on show stats.
        for (int i = 0; i < JourneyLogger.CMP_SHOW_MAX; ++i) {
            Assert.assertEquals(0,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "PaymentRequest.CanMakePayment.Used.EffectOnShow", i));
        }

        // Effect on completion stats.
        for (int i = 0; i < JourneyLogger.COMPLETION_STATUS_MAX; ++i) {
            Assert.assertEquals(0,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion", i));
            Assert.assertEquals(0,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "PaymentRequest.CanMakePayment.Used.TrueWithShowEffectOnCompletion",
                            i));
            Assert.assertEquals(0,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
                            i));
        }
    }
}
