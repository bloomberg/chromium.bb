// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.HAVE_INSTRUMENTS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.IMMEDIATE_RESPONSE;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.PaymentAppFactory.PaymentAppCreatedCallback;
import org.chromium.chrome.browser.payments.PaymentAppFactory.PaymentAppFactoryAddition;
import org.chromium.chrome.browser.payments.PaymentRequestTestCommon.TestPay;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;

import java.util.Arrays;
import java.util.Set;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** A payment integration test that sorting payment apps and instruments by frecency. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
public class PaymentRequestPaymentAppsSortingTest implements MainActivityStartCallback {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule = new PaymentRequestTestRule(
            "payment_request_alicepay_bobpay_charliepay_and_cards_test.html", this);

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "310-310-6000", "jon.doe@gmail.com", "en-US"));
        // Visa card with complete set of information. This payment method is always listed
        // behind non-autofill payment instruments in payment request.
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "", "12", "2050", "visa", R.drawable.visa_card,
                billingAddressId, "" /* serverId */));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentAppsSortingByFrecency()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Install a payment app with Bob Pay and Alice Pay, and another payment app with Charlie
        // Pay.
        final TestPay appA =
                new TestPay(Arrays.asList("https://alicepay.com", "https://bobpay.com"),
                        HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        final TestPay appB = new TestPay(
                Arrays.asList("https://charliepay.com"), HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        PaymentAppFactory.getInstance().addAdditionalFactory(new PaymentAppFactoryAddition() {
            @Override
            public void create(WebContents webContents, Set<String> methodNames,
                    PaymentAppCreatedCallback callback) {
                callback.onPaymentAppCreated(appA);
                callback.onPaymentAppCreated(appB);
                callback.onAllPaymentAppsCreated();
            }
        });
        String appAAlicePayId = appA.getAppIdentifier() + "https://alicepay.com";
        String appABobPayId = appA.getAppIdentifier() + "https://bobpay.com";
        String appBCharliePayId = appB.getAppIdentifier() + "https://charliepay.com";

        // The initial records for all payment methods are zeroes.
        Assert.assertEquals(0, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appAAlicePayId));
        Assert.assertEquals(0, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appABobPayId));
        Assert.assertEquals(
                0, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appBCharliePayId));
        Assert.assertEquals(
                0, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appAAlicePayId));
        Assert.assertEquals(
                0, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appABobPayId));
        Assert.assertEquals(
                0, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appBCharliePayId));

        // Sets Alice Pay use count and use date to 5. Sets Bob Pay use count and use date to 10.
        // Sets Charlie Pay use count and use date to 15.
        PaymentPreferencesUtil.setPaymentInstrumentUseCountForTest(appAAlicePayId, 5);
        PaymentPreferencesUtil.setPaymentInstrumentLastUseDate(appAAlicePayId, 5);
        PaymentPreferencesUtil.setPaymentInstrumentUseCountForTest(appABobPayId, 10);
        PaymentPreferencesUtil.setPaymentInstrumentLastUseDate(appABobPayId, 10);
        PaymentPreferencesUtil.setPaymentInstrumentUseCountForTest(appBCharliePayId, 15);
        PaymentPreferencesUtil.setPaymentInstrumentLastUseDate(appBCharliePayId, 15);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Checks Charlie Pay is listed at the first position.
        Assert.assertEquals(4, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        Assert.assertEquals(
                "https://charliepay.com", mPaymentRequestTestRule.getPaymentInstrumentLabel(0));
        Assert.assertEquals(
                "https://bobpay.com", mPaymentRequestTestRule.getPaymentInstrumentLabel(1));
        Assert.assertEquals(
                "https://alicepay.com", mPaymentRequestTestRule.getPaymentInstrumentLabel(2));
        Assert.assertEquals(
                "Visa\u0020\u0020\u2022\u2006\u2022\u2006\u2022\u2006\u2022\u20061111\nJon Doe",
                mPaymentRequestTestRule.getPaymentInstrumentLabel(3));

        // Cancel the Payment Request.
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_secondary, mPaymentRequestTestRule.getDismissed());

        // Checks the records for all payment instruments haven't been changed.
        Assert.assertEquals(5, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appAAlicePayId));
        Assert.assertEquals(10, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appABobPayId));
        Assert.assertEquals(
                15, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appBCharliePayId));
        Assert.assertEquals(
                5, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appAAlicePayId));
        Assert.assertEquals(
                10, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appABobPayId));
        Assert.assertEquals(
                15, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appBCharliePayId));

        // Sets Alice Pay use count and use date to 20.
        PaymentPreferencesUtil.setPaymentInstrumentUseCountForTest(appAAlicePayId, 20);
        PaymentPreferencesUtil.setPaymentInstrumentLastUseDate(appAAlicePayId, 20);

        mPaymentRequestTestRule.reTriggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Checks Alice Pay is listed at the first position. Checks Bob Pay is listed at the second
        // position together with Alice Pay since they come from the same app.
        Assert.assertEquals(4, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        Assert.assertEquals(
                "https://alicepay.com", mPaymentRequestTestRule.getPaymentInstrumentLabel(0));
        Assert.assertEquals(
                "https://bobpay.com", mPaymentRequestTestRule.getPaymentInstrumentLabel(1));
        Assert.assertEquals(
                "https://charliepay.com", mPaymentRequestTestRule.getPaymentInstrumentLabel(2));
        Assert.assertEquals(
                "Visa\u0020\u0020\u2022\u2006\u2022\u2006\u2022\u2006\u2022\u20061111\nJon Doe",
                mPaymentRequestTestRule.getPaymentInstrumentLabel(3));

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        // Checks Alice Pay is selected as the default payment method.
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://alicepay.com", "\"transaction\"", "1337"});

        // Checks Alice Pay use count is increased by one after completing a payment request with
        // it.
        Assert.assertEquals(
                21, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appAAlicePayId));
        Assert.assertEquals(10, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appABobPayId));
        Assert.assertEquals(
                15, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appBCharliePayId));
        Assert.assertTrue(
                PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appAAlicePayId) > 20);
        Assert.assertEquals(
                10, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appABobPayId));
        Assert.assertEquals(
                15, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appBCharliePayId));
    }
}
