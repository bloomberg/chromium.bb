// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.support.test.filters.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for the correct log of the CanMakePayment metrics.
 */
public class PaymentRequestCanMakePaymentMetricsTest extends PaymentRequestTestBase {
    public PaymentRequestCanMakePaymentMetricsTest() {
        super("payment_request_can_make_payment_metrics_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        helper.setProfile(new AutofillProfile("", "https://example.com", true, "Jon Doe", "Google",
                "340 Main St", "CA", "Los Angeles", "", "90291", "", "US", "310-310-6000",
                "jon.doe@gmail.com", "en-US"));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * calling it, receiving no as a response, still showing the Payment Request and the user aborts
     * the flow.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testCannotMakePayment_Abort()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Initiate a payment request.
        triggerUIAndWait("queryShow", getReadyForInput());

        // Press the back button.
        int callCount = getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getPaymentRequestUI().getDialogForTest().onBackPressed();
            }
        });
        getDismissed().waitForCallback(callCount);
        expectResultContains(new String[] {"Request cancelled"});

        // CanMakePayment was queried.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being false and shown.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.EffectOnShow",
                        JourneyLogger.CMP_SHOW_DID_SHOW));

        // There should be a record for an abort when CanMakePayment is false but the PR is shown to
        // the user.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_USER_ABORTED));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * calling it, receiving no as a response, still showing the Payment Request and the user
     * completes the flow.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testCannotMakePayment_Complete()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait("queryShow", getReadyForInput());

        // Add a new credit card.
        clickInPaymentMethodAndWait(R.id.payments_section, getReadyToEdit());
        setSpinnerSelectionsInCardEditorAndWait(
                new int[] {11, 1, 0}, getBillingAddressChangeProcessed());
        setTextInCardEditorAndWait(
                new String[] {"4111111111111111", "Jon Doe"}, getEditorTextUpdate());
        clickInCardEditorAndWait(R.id.payments_edit_done_button, getReadyToPay());

        // Complete the transaction.
        clickAndWait(R.id.button_primary, getReadyForUnmaskInput());
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", getReadyToUnmask());
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, getDismissed());

        // CanMakePayment was queried.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being false and shown.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.EffectOnShow",
                        JourneyLogger.CMP_SHOW_DID_SHOW));

        // There should be a record for a completion when CanMakePayment is false but the PR is
        // shown to the user.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * calling it, receiving yeas as a response, showing the Payment Request and the user aborts the
     * flow.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testCanMakePayment_Abort()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Install the app so CanMakePayment returns true.
        installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);

        // Initiate a payment request.
        triggerUIAndWait("queryShow", getReadyForInput());

        // Press the back button.
        int callCount = getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getPaymentRequestUI().getDialogForTest().onBackPressed();
            }
        });
        getDismissed().waitForCallback(callCount);
        expectResultContains(new String[] {"Request cancelled"});

        // CanMakePayment was queried.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being false and shown.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.EffectOnShow",
                        JourneyLogger.CMP_SHOW_DID_SHOW
                                | JourneyLogger.CMP_SHOW_COULD_MAKE_PAYMENT));

        // There should be a record for an abort when CanMakePayment is false but the PR is shown to
        // the user.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.TrueWithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_USER_ABORTED));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * calling it, receiving yeas as a response, showing the Payment Request and the user completes
     * the flow.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testCanMakePayment_Complete()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Install the app so CanMakePayment returns true.
        installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);

        // Initiate an complete a payment request.
        triggerUIAndWait("queryShow", getReadyForInput());
        clickAndWait(R.id.button_primary, getDismissed());

        // CanMakePayment was queried.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being false and shown.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.EffectOnShow",
                        JourneyLogger.CMP_SHOW_DID_SHOW
                                | JourneyLogger.CMP_SHOW_COULD_MAKE_PAYMENT));

        // There should be a record for an abort when CanMakePayment is false but the PR is shown to
        // the user.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.TrueWithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_COMPLETED));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * not calling it but still showing the Payment Request and the user aborts the flow.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNoQuery_Abort()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Initiate a payment request.
        triggerUIAndWait("noQueryShow", getReadyForInput());

        // Press the back button.
        int callCount = getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getPaymentRequestUI().getDialogForTest().onBackPressed();
            }
        });
        getDismissed().waitForCallback(callCount);
        expectResultContains(new String[] {"Request cancelled"});

        // CanMakePayment was not queried.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_NOT_USED));

        // There should be a record for an abort when CanMakePayment is not called but the PR is
        // shown to the user.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_USER_ABORTED));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * not calling it but still showing the Payment Request and the user completes the flow.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testNoQuery_Completes()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Install the app so the user can complete the Payment Request.
        installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);

        // Initiate a payment request.
        triggerUIAndWait("noQueryShow", getReadyForInput());
        clickAndWait(R.id.button_primary, getDismissed());

        // CanMakePayment was not queried.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        JourneyLogger.CAN_MAKE_PAYMENT_NOT_USED));

        // There should be a record for a completion when CanMakePayment is not called but the PR is
        // shown to the user.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                        JourneyLogger.COMPLETION_STATUS_COMPLETED));
    }
}
