// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.test.util.ChromeTabUtils;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test to validate the logging of Payment Request metrics.
 */
public class PaymentRequestMetricsTest extends PaymentRequestTestBase {
    public PaymentRequestMetricsTest() {
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
    }

    /**
     * Expect that the successful checkout funnel metrics are logged during a succesful checkout.
     */
    @MediumTest
    public void testSuccessCheckoutFunnel() throws InterruptedException, ExecutionException,
            TimeoutException {
        // Initiate a payment request.
        triggerUIAndWait(mReadyToPay);

        // Make sure sure that the "Initiated" and "Shown" events were logged.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.CheckoutFunnel.Initiated", 1));
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.CheckoutFunnel.Shown", 1));

        // Click the pay button.
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);

        // Make sure sure that the "PayClicked" event was logged.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.CheckoutFunnel.PayClicked", 1));

        // Unmask the credit card,
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);

        // Make sure sure that the "ReceivedInstrumentDetails" and "Completed" events were logged.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.CheckoutFunnel.ReceivedInstrumentDetails", 1));
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.CheckoutFunnel.Completed", 1));
    }

    /**
     * Expect only the ABORT_REASON_ABORTED_BY_USER enum value gets logged when a user cancels a
     * Payment Request.
     */
    @MediumTest
    public void testAbortMetrics_AbortedByUser_CancelButton() throws InterruptedException,
            ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);

        // Cancel the Payment Request.
        int callCount = mDismissed.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getDialogForTest().findViewById(R.id.button_secondary).performClick();
            }
        });
        mDismissed.waitForCallback(callCount);
        expectResultContains(new String[] {"Request cancelled"});

        assertOnlySpecificAbortMetricLogged(
                PaymentRequestMetrics.ABORT_REASON_ABORTED_BY_USER);
    }

    /**
     * Expect only the ABORT_REASON_ABORTED_BY_USER enum value gets logged when a user presses on
     * the [X] button in the Payment Request dialog.
     */
    @MediumTest
    public void testAbortMetrics_AbortedByUser_XButton() throws InterruptedException,
            ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);

        // Press the [X] button.
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});

        assertOnlySpecificAbortMetricLogged(
                PaymentRequestMetrics.ABORT_REASON_ABORTED_BY_USER);
    }

    /**
     * Expect only the ABORT_REASON_ABORTED_BY_USER enum value gets logged when a user presses on
     * the back button on their phone during a Payment Request.
     */
    @MediumTest
    public void testAbortMetrics_AbortedByUser_BackButton() throws InterruptedException,
            ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);

        // Press the back button.
        int callCount = mDismissed.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getDialogForTest().onBackPressed();
            }
        });
        mDismissed.waitForCallback(callCount);
        expectResultContains(new String[] {"Request cancelled"});

        assertOnlySpecificAbortMetricLogged(
                PaymentRequestMetrics.ABORT_REASON_ABORTED_BY_USER);
    }

    /**
     * Expect only the ABORT_REASON_MOJO_RENDERER_CLOSING enum value gets logged when a user closes
     * the tab during a Payment Request.
     */
    @MediumTest
    public void testAbortMetrics_AbortedByUser_TabClosed() throws InterruptedException,
            ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);

        // Press the back button.
        ChromeTabUtils.closeCurrentTab(getInstrumentation(), getActivity());

        assertOnlySpecificAbortMetricLogged(
                PaymentRequestMetrics.ABORT_REASON_MOJO_RENDERER_CLOSING);
    }

    /**
     * Expect only the ABORT_REASON_ABORTED_BY_MERCHANT enum value gets logged when a Payment
     * Request gets cancelled by the merchant.
     */
    @MediumTest
    public void testAbortMetrics_AbortedByMerchant() throws InterruptedException,
            ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);

        // Simulate an abort by the merchant.
        clickNodeAndWait("abort", mDismissed);
        expectResultContains(new String[] {"Abort"});

        assertOnlySpecificAbortMetricLogged(
                PaymentRequestMetrics.ABORT_REASON_ABORTED_BY_MERCHANT);
    }

    /**
     * Expect only the ABORT_REASON_NO_MATCHING_PAYMENT_METHOD enum value gets logged when a Payment
     * Request gets cancelled because the user does not have any of the payment methods accepted by
     * the merchant and the merchant does not accept credit cards.
     */
    @MediumTest
    public void testAbortMetrics_NoMatchingPaymentMethod() throws InterruptedException,
            ExecutionException, TimeoutException {
        // Bob pay is supported but no instruments are present.
        installPaymentApp(NO_INSTRUMENTS, DELAYED_RESPONSE);
        triggerUIAndWait("noMatching", mShowFailed);
        expectResultContains(new String[] {"The payment method is not supported"});

        assertOnlySpecificAbortMetricLogged(
                PaymentRequestMetrics.ABORT_REASON_NO_MATCHING_PAYMENT_METHOD);
    }

    /**
     * Expect only the ABORT_REASON_NO_SUPPORTED_PAYMENT_METHOD enum value gets logged when a
     * Payment Request gets cancelled because the merchant only accepts payment methods we don't
     * support.
     */
    @MediumTest
    public void testAbortMetrics_NoSupportedPaymentMethod() throws InterruptedException,
            ExecutionException, TimeoutException {
        triggerUIAndWait("noSupported", mShowFailed);
        expectResultContains(new String[] {"The payment method is not supported"});

        assertOnlySpecificAbortMetricLogged(
                PaymentRequestMetrics.ABORT_REASON_NO_SUPPORTED_PAYMENT_METHOD);
    }

    /**
     * Asserts that only the specified reason for abort is logged.
     *
     * @param abortReason The only bucket in the abort histogram that should have a record.
     */
    private void assertOnlySpecificAbortMetricLogged(int abortReason) {
        for (int i = 0; i < PaymentRequestMetrics.ABORT_REASON_MAX; ++i) {
            assertEquals((i == abortReason ? 1 : 0),
                    RecordHistogram.getHistogramValueCountForTesting(
                            "PaymentRequest.CheckoutFunnel.Aborted", i));
        }
    }
}
