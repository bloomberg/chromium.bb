// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;

import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.DELAYED_RESPONSE;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.HAVE_INSTRUMENTS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.IMMEDIATE_RESPONSE;

import android.support.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests payment via Bob Pay or basic-card with
 * modifiers.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
public class PaymentRequestPaymentAppAndBasicCardWithModifiersTest
        implements PaymentRequestTestRule.MainActivityStartCallback {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule = new PaymentRequestTestRule(
            "payment_request_bobpay_and_basic_card_with_modifiers_test.html", this);

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "310-310-6000", "jon.doe@gmail.com", "en-US"));
        // Mastercard card without a billing address.
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "5454545454545454", "", "12", "2050", "mastercard", R.drawable.mc_card,
                CardType.UNKNOWN, "" /* billingAddressId */, "" /* serverId */));
        // Visa card with complete set of information.
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "", "12", "2050", "visa", R.drawable.visa_card,
                CardType.UNKNOWN, billingAddressId, "" /* serverId */));
    }

    /**
     * Verify modifier for Bobpay is only applied for Bobpay.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateTotalAndInstrumentLabelWithModifiers()
            throws InterruptedException, ExecutionException, TimeoutException {
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());

        assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith(
                "https://bobpay.com"));
        assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        // select other payment method and verify modifier for bobpay is not applied
        mPaymentRequestTestRule.clickOnPaymentMethodSuggestionOptionAndWait(
                1, mPaymentRequestTestRule.getReadyForInput());
        assertFalse(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith(
                "https://bobpay.com"));
        assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    /**
     * Verify native app can pay as expected when modifier is applied.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentAppCanPayWithModifiers()
            throws InterruptedException, ExecutionException, TimeoutException {
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, DELAYED_RESPONSE);
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());

        assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith(
                "https://bobpay.com"));
        assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.com", "\"transaction\"", "1337"});
    }
}
