// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.PaymentAppFactory.PaymentAppCreatedCallback;
import org.chromium.chrome.browser.payments.PaymentAppFactory.PaymentAppFactoryAddition;
import org.chromium.content_public.browser.WebContents;

import java.util.Arrays;
import java.util.Set;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** A payment integration test that sorting payment apps and instruments by frecency. */
public class PaymentRequestPaymentAppsSortingTest extends PaymentRequestTestBase {
    public PaymentRequestPaymentAppsSortingTest() {
        super("payment_request_alicepay_bobpay_charliepay_and_cards_test.html");
    }

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
                "4111111111111111", "", "12", "2050", "visa", R.drawable.pr_visa, billingAddressId,
                "" /* serverId */));
    }

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
        assertEquals(0, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appAAlicePayId));
        assertEquals(0, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appABobPayId));
        assertEquals(0, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appBCharliePayId));
        assertEquals(0, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appAAlicePayId));
        assertEquals(0, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appABobPayId));
        assertEquals(0, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appBCharliePayId));

        // Sets Alice Pay use count and use date to 5. Sets Bob Pay use count and use date to 10.
        // Sets Charlie Pay use count and use date to 15.
        PaymentPreferencesUtil.setPaymentInstrumentUseCountForTest(appAAlicePayId, 5);
        PaymentPreferencesUtil.setPaymentInstrumentLastUseDate(appAAlicePayId, 5);
        PaymentPreferencesUtil.setPaymentInstrumentUseCountForTest(appABobPayId, 10);
        PaymentPreferencesUtil.setPaymentInstrumentLastUseDate(appABobPayId, 10);
        PaymentPreferencesUtil.setPaymentInstrumentUseCountForTest(appBCharliePayId, 15);
        PaymentPreferencesUtil.setPaymentInstrumentLastUseDate(appBCharliePayId, 15);

        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);

        // Checks Charlie Pay is listed at the first position.
        assertEquals(4, getNumberOfPaymentInstruments());
        assertEquals("https://charliepay.com", getPaymentInstrumentLabel(0));
        assertEquals("https://bobpay.com", getPaymentInstrumentLabel(1));
        assertEquals("https://alicepay.com", getPaymentInstrumentLabel(2));
        assertEquals(
                "Visa\u0020\u0020\u2022\u2006\u2022\u2006\u2022\u2006\u2022\u20061111\nJon Doe",
                getPaymentInstrumentLabel(3));

        // Cancel the Payment Request.
        clickAndWait(R.id.button_secondary, mDismissed);

        // Checks the records for all payment instruments haven't been changed.
        assertEquals(5, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appAAlicePayId));
        assertEquals(10, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appABobPayId));
        assertEquals(15, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appBCharliePayId));
        assertEquals(5, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appAAlicePayId));
        assertEquals(10, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appABobPayId));
        assertEquals(15, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appBCharliePayId));

        // Sets Alice Pay use count and use date to 20.
        PaymentPreferencesUtil.setPaymentInstrumentUseCountForTest(appAAlicePayId, 20);
        PaymentPreferencesUtil.setPaymentInstrumentLastUseDate(appAAlicePayId, 20);

        reTriggerUIAndWait("buy", mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);

        // Checks Alice Pay is listed at the first position. Checks Bob Pay is listed at the second
        // position together with Alice Pay since they come from the same app.
        assertEquals(4, getNumberOfPaymentInstruments());
        assertEquals("https://alicepay.com", getPaymentInstrumentLabel(0));
        assertEquals("https://bobpay.com", getPaymentInstrumentLabel(1));
        assertEquals("https://charliepay.com", getPaymentInstrumentLabel(2));
        assertEquals(
                "Visa\u0020\u0020\u2022\u2006\u2022\u2006\u2022\u2006\u2022\u20061111\nJon Doe",
                getPaymentInstrumentLabel(3));

        clickAndWait(R.id.button_primary, mDismissed);
        // Checks Alice Pay is selected as the default payment method.
        expectResultContains(new String[] {"https://alicepay.com", "\"transaction\"", "1337"});

        // Checks Alice Pay use count is increased by one after completing a payment request with
        // it.
        assertEquals(21, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appAAlicePayId));
        assertEquals(10, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appABobPayId));
        assertEquals(15, PaymentPreferencesUtil.getPaymentInstrumentUseCount(appBCharliePayId));
        assertTrue(PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appAAlicePayId) > 20);
        assertEquals(10, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appABobPayId));
        assertEquals(15, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(appBCharliePayId));
    }
}
