// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.HAVE_INSTRUMENTS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.IMMEDIATE_RESPONSE;

import android.content.DialogInterface;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for the correct log of the CanMakePayment metrics.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
public class PaymentRequestCanMakePaymentMetricsTest implements MainActivityStartCallback {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_can_make_payment_metrics_test.html", this);

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
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCannotMakePayment_UserAbort()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShow", mPaymentRequestTestRule.getReadyForInput());

        // Press the back button.
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mPaymentRequestTestRule.getPaymentRequestUI().getDialogForTest().onBackPressed();
            }
        });
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);
        mPaymentRequestTestRule.expectResultContains(new String[] {"Request cancelled"});

        // CanMakePayment was queried.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        CanMakePaymentUsage.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being false and shown.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.EffectOnShow",
                        CanMakePaymentEffectOnShow.DID_SHOW));

        // There should be a record for an abort when CanMakePayment is false but the PR is shown to
        // the user.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
                        CompletionStatus.USER_ABORTED));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * calling it, receiving no as a response, still showing the Payment Request and the user
     * completes the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCannotMakePayment_Complete()
            throws InterruptedException, ExecutionException, TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShow", mPaymentRequestTestRule.getReadyForInput());

        // Add a new credit card.
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyToEdit());
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

        // CanMakePayment was queried.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        CanMakePaymentUsage.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being false and shown.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.EffectOnShow",
                        CanMakePaymentEffectOnShow.DID_SHOW));

        // There should be a record for a completion when CanMakePayment is false but the PR is
        // shown to the user.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
                        CompletionStatus.COMPLETED));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * calling it, receiving yes as a response, showing the Payment Request and the merchant aborts
     * the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanMakePayment_MerchantAbort()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Install the app so CanMakePayment returns true.
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);

        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShow", mPaymentRequestTestRule.getReadyForInput());

        // Simulate an abort by the merchant.
        mPaymentRequestTestRule.clickNodeAndWait("abort", mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"Abort"});

        // CanMakePayment was queried.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        CanMakePaymentUsage.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being false and shown.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.EffectOnShow",
                        CanMakePaymentEffectOnShow.DID_SHOW
                                | CanMakePaymentEffectOnShow.COULD_MAKE_PAYMENT));

        // There should be a record for an abort when CanMakePayment is false but the PR is shown to
        // the user.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.TrueWithShowEffectOnCompletion",
                        CompletionStatus.OTHER_ABORTED));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * calling it, receiving yes as a response, showing the Payment Request and the user completes
     * the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanMakePayment_Complete()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Install the app so CanMakePayment returns true.
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);

        // Initiate an complete a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShow", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // CanMakePayment was queried.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        CanMakePaymentUsage.CAN_MAKE_PAYMENT_USED));

        // The CanMakePayment effect on show should be recorded as being false and shown.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.EffectOnShow",
                        CanMakePaymentEffectOnShow.DID_SHOW
                                | CanMakePaymentEffectOnShow.COULD_MAKE_PAYMENT));

        // There should be a record for an abort when CanMakePayment is false but the PR is shown to
        // the user.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Used.TrueWithShowEffectOnCompletion",
                        CompletionStatus.COMPLETED));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * not calling it but still showing the Payment Request and the user aborts the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoQuery_UserAbort()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "noQueryShow", mPaymentRequestTestRule.getReadyForInput());

        // Press the back button.
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mPaymentRequestTestRule.getPaymentRequestUI().getDialogForTest().onBackPressed();
            }
        });
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);
        mPaymentRequestTestRule.expectResultContains(new String[] {"Request cancelled"});

        // CanMakePayment was not queried.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        CanMakePaymentUsage.CAN_MAKE_PAYMENT_NOT_USED));

        // There should be a record for an abort when CanMakePayment is not called but the PR is
        // shown to the user.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                        CompletionStatus.USER_ABORTED));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * not calling it but still showing the Payment Request and the user completes the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoQuery_Complete()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Install the app so the user can complete the Payment Request.
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);

        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "noQueryShow", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // CanMakePayment was not queried.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.Usage",
                        CanMakePaymentUsage.CAN_MAKE_PAYMENT_NOT_USED));

        // There should be a record for a completion when CanMakePayment is not called but the PR is
        // shown to the user.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
                        CompletionStatus.COMPLETED));
    }
}
