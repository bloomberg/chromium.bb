// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;
import android.view.View;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for auto advancing to next field when typing in card numbers.
 *
 * Below valid test card numbers are from https://stripe.com/docs/testing#cards and
 * https://developers.braintreepayments.com/guides/unionpay/testing/javascript/v3
 */
public class PaymentRequestCardEditorAutoAdvanceTest extends PaymentRequestTestBase {
    public PaymentRequestCardEditorAutoAdvanceTest() {
        super("payment_request_free_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // Set user has a shipping address and valid credit card on disk to make it easy to click in
        // to the payment section.
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "1", "2050", "visa", R.drawable.pr_visa,
                billingAddressId, "" /* serverId */));
    }

    @MediumTest
    @Feature({"Payments"})
    public void test14DigitsCreditCard()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);

        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, mReadyToEdit);

        // Diners credit card.
        final View focusedChildView = getCardEditorFocusedView();
        setTextInCardEditorAndWait(new String[] {"3056 9309 0259 0"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() == focusedChildView);

        // '3056 9309 0259 00' is an invalid 14 digits card number.
        setTextInCardEditorAndWait(new String[] {"3056 9309 0259 00"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() == focusedChildView);

        // '3056 9309 0259 04' is a valid 14 digits card number.
        setTextInCardEditorAndWait(new String[] {"3056 9309 0259 04"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() != focusedChildView);

        // Request focus to card number field after auto advancing above.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                focusedChildView.requestFocus();
            }
        });
        setTextInCardEditorAndWait(new String[] {"3056 9309 0259 041"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() == focusedChildView);
    }

    @MediumTest
    @Feature({"Payments"})
    public void test15DigitsCreditCard()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);

        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, mReadyToEdit);

        // American Express credit card.
        final View focusedChildView = getCardEditorFocusedView();
        setTextInCardEditorAndWait(new String[] {"3782 822463 1000"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() == focusedChildView);

        // '3782 822463 10000' is an invalid 15 digits card number.
        setTextInCardEditorAndWait(new String[] {"3782 822463 10000"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() == focusedChildView);

        // '3782 822463 10005' is a valid 15 digits card number.
        setTextInCardEditorAndWait(new String[] {"3782 822463 10005"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() != focusedChildView);

        // Request focus to card number field after auto advancing above.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                focusedChildView.requestFocus();
            }
        });
        setTextInCardEditorAndWait(new String[] {"3782 822463 10005 1"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() == focusedChildView);
    }

    @MediumTest
    @Feature({"Payments"})
    public void test16DigitsCreditCard()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);

        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, mReadyToEdit);

        // DISCOVER, JCB, MASTERCARD, MIR and VISA cards have 16 digits. Takes VISA as test input
        // which has 13 digits valid card.
        final View focusedChildView = getCardEditorFocusedView();
        setTextInCardEditorAndWait(new String[] {"4012 8888 8888 "}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() == focusedChildView);

        // '4012 8888 8888 1' is a valid 13 digits card number.
        setTextInCardEditorAndWait(new String[] {"4012 8888 8888 1"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() == focusedChildView);

        setTextInCardEditorAndWait(new String[] {"4012 8888 8888 188"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() == focusedChildView);

        // '4012 8888 8888 1880' is an invalid 16 digits card number.
        setTextInCardEditorAndWait(new String[] {"4012 8888 8888 1880"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() == focusedChildView);

        // '4012 8888 8888 1881' is a valid 16 digits card number.
        setTextInCardEditorAndWait(new String[] {"4012 8888 8888 1881"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() != focusedChildView);

        // Request focus to card number field after auto advancing above.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                focusedChildView.requestFocus();
            }
        });
        setTextInCardEditorAndWait(new String[] {"4012 8888 8888 1881 1"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() == focusedChildView);
    }

    @MediumTest
    @Feature({"Payments"})
    public void test19DigitsCreditCard()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);

        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, mReadyToEdit);

        // UNIONPAY credit card.
        final View focusedChildView = getCardEditorFocusedView();
        setTextInCardEditorAndWait(new String[] {"6250 9410 0652 859"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() == focusedChildView);

        // '6250 9410 0652 8599' is a valid 16 digits card number.
        setTextInCardEditorAndWait(new String[] {"6250 9410 0652 8599"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() == focusedChildView);

        setTextInCardEditorAndWait(new String[] {"6212 3456 7890 0000 00"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() == focusedChildView);

        // '6212 3456 7890 0000 001' is an invalid 19 digits card number.
        setTextInCardEditorAndWait(new String[] {"6212 3456 7890 0000 001"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() == focusedChildView);

        // '6212 3456 7890 0000 003' is a valid 19 digits card number.
        setTextInCardEditorAndWait(new String[] {"6212 3456 7890 0000 003"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() != focusedChildView);

        // Request focus to card number field after auto advancing above.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                focusedChildView.requestFocus();
            }
        });
        setTextInCardEditorAndWait(new String[] {"6212 3456 7890 0000 0031"}, mEditorTextUpdate);
        assertTrue(getCardEditorFocusedView() == focusedChildView);
    }
}
