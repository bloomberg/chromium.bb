// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.DECEMBER;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.FIRST_BILLING_ADDRESS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.HAVE_INSTRUMENTS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.IMMEDIATE_RESPONSE;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.NEXT_YEAR;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.ClassRule;
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
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DisableAnimationsTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for the correct log of the CanMakePayment metrics.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=PaymentRequestHasEnrolledInstrument"})
public class PaymentRequestCanMakePaymentMetricsTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_can_make_payment_metrics_test.html", this);

    @Override
    public void onMainActivityStarted() throws InterruptedException, TimeoutException {
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
    @CommandLineFlags.Add("disable-features=NoCreditCardAbort")
    public void testCannotMakePayment_UserAbort() throws InterruptedException, TimeoutException {
        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShow", mPaymentRequestTestRule.getReadyForInput());

        // Press the back button.
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPaymentRequestTestRule.getPaymentRequestUI().getDialogForTest()
                        .onBackPressed());
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);
        mPaymentRequestTestRule.expectResultContains(new String[] {"Request cancelled"});

        // Make sure the canMakePayment events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.CAN_MAKE_PAYMENT_TRUE
                | Event.HAS_ENROLLED_INSTRUMENT_FALSE | Event.REQUEST_METHOD_BASIC_CARD
                | Event.REQUEST_METHOD_OTHER;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * calling it, receiving no as a response, still showing the Payment Request and the user
     * completes the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add("disable-features=NoCreditCardAbort")
    public void testCannotMakePayment_Complete() throws InterruptedException, TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShow", mPaymentRequestTestRule.getReadyForInput());

        // Add a new credit card.
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS},
                mPaymentRequestTestRule.getBillingAddressChangeProcessed());
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"4111111111111111", "Jon Doe"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInCardEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());

        // Complete the transaction.
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the canMakePayment events were logged correctly.
        int expectedSample = Event.SHOWN | Event.PAY_CLICKED | Event.RECEIVED_INSTRUMENT_DETAILS
                | Event.COMPLETED | Event.CAN_MAKE_PAYMENT_TRUE
                | Event.HAS_ENROLLED_INSTRUMENT_FALSE | Event.REQUEST_METHOD_BASIC_CARD
                | Event.REQUEST_METHOD_OTHER | Event.SELECTED_CREDIT_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * calling it, receiving yes as a response, showing the Payment Request and the merchant aborts
     * the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanMakePayment_MerchantAbort() throws InterruptedException, TimeoutException {
        // Install the app so CanMakePayment returns true.
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);

        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShow", mPaymentRequestTestRule.getReadyForInput());

        // Simulate an abort by the merchant.
        mPaymentRequestTestRule.clickNodeAndWait("abort", mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"Abort"});

        // Make sure the canMakePayment events were logged correctly.
        int expectedSample = Event.SHOWN | Event.OTHER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.CAN_MAKE_PAYMENT_TRUE
                | Event.HAS_ENROLLED_INSTRUMENT_TRUE | Event.REQUEST_METHOD_BASIC_CARD
                | Event.REQUEST_METHOD_OTHER;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * calling it, receiving yes as a response, showing the Payment Request and the user completes
     * the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanMakePayment_Complete() throws InterruptedException, TimeoutException {
        // Install the app so CanMakePayment returns true.
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);

        // Initiate an complete a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShow", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // Make sure the canMakePayment events were logged correctly.
        int expectedSample = Event.SHOWN | Event.PAY_CLICKED | Event.RECEIVED_INSTRUMENT_DETAILS
                | Event.COMPLETED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.CAN_MAKE_PAYMENT_TRUE
                | Event.HAS_ENROLLED_INSTRUMENT_TRUE | Event.REQUEST_METHOD_BASIC_CARD
                | Event.REQUEST_METHOD_OTHER | Event.SELECTED_OTHER;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * calling it, receiving false as a response because can make payment is disabled, showing the
     * Payment Request and the user completes the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanMakePaymentDisabled_Complete()
            throws InterruptedException, TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking((Runnable) () -> {
            PrefServiceBridge.getInstance().setBoolean(Pref.CAN_MAKE_PAYMENT_ENABLED, false);
        });

        // Install the app so CanMakePayment returns true if it is enabled.
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);

        // Initiate an complete a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShow", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // Make sure the canMakePayment events were logged correctly.
        int expectedSample = Event.SHOWN | Event.PAY_CLICKED | Event.RECEIVED_INSTRUMENT_DETAILS
                | Event.COMPLETED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.CAN_MAKE_PAYMENT_FALSE
                | Event.HAS_ENROLLED_INSTRUMENT_FALSE | Event.REQUEST_METHOD_BASIC_CARD
                | Event.REQUEST_METHOD_OTHER | Event.SELECTED_OTHER;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * not calling it but still showing the Payment Request and the user aborts the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add("disable-features=NoCreditCardAbort")
    public void testNoQuery_UserAbort() throws InterruptedException, TimeoutException {
        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "noQueryShow", mPaymentRequestTestRule.getReadyForInput());

        // Press the back button.
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPaymentRequestTestRule.getPaymentRequestUI().getDialogForTest()
                        .onBackPressed());
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);
        mPaymentRequestTestRule.expectResultContains(new String[] {"Request cancelled"});

        // Make sure no canMakePayment events were logged.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.REQUEST_METHOD_BASIC_CARD
                | Event.REQUEST_METHOD_OTHER;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * not calling it but still showing the Payment Request and the user completes the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoQuery_Complete() throws InterruptedException, TimeoutException {
        // Install the app so the user can complete the Payment Request.
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);

        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "noQueryShow", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // Make sure no canMakePayment events were logged.
        int expectedSample = Event.SHOWN | Event.PAY_CLICKED | Event.RECEIVED_INSTRUMENT_DETAILS
                | Event.COMPLETED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_METHOD_BASIC_CARD
                | Event.REQUEST_METHOD_OTHER | Event.SELECTED_OTHER;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }
}
