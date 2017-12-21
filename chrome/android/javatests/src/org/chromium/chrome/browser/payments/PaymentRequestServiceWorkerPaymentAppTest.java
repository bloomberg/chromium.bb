// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.graphics.drawable.ColorDrawable;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.payments.mojom.BasicCardNetwork;
import org.chromium.payments.mojom.BasicCardType;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

import javax.annotation.Nullable;

/**
 * A payment integration test for service worker based payment apps.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        // For all the tests in this file, we expect abort exception when there is no supported
        // payment instruments instead of showing payment request UI.
        "enable-features=" + ChromeFeatureList.NO_CREDIT_CARD_ABORT})
public class PaymentRequestServiceWorkerPaymentAppTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule = new PaymentRequestTestRule(
            "payment_request_bobpay_and_basic_card_with_modifier_optional_data_test.html");

    /**
     * Installs a mock service worker based payment app for testing.
     *
     * @param supportedMethodNames The supported payment methods of the mock payment app.
     * @param capabilities         The capabilities of the mocked payment app.
     * @param name                 The payment app name.
     * @param icon                 The payment app icon.
     */
    private void installMockServiceWorkerPaymentApp(final String[] supportedMethodNames,
            final ServiceWorkerPaymentApp.Capabilities[] capabilities, @Nullable final String name,
            @Nullable final ColorDrawable icon) {
        PaymentAppFactory.getInstance().addAdditionalFactory(
                (webContents, methodNames, callback) -> {
                    callback.onPaymentAppCreated(new ServiceWorkerPaymentApp(webContents,
                            0 /* registrationId */,
                            UriUtils.parseUriFromString("https://bobpay.com") /* scope */,
                            name /* name */, "test@bobpay.com" /* userHint */,
                            "https://bobpay.com" /* origin */, icon /* icon */,
                            supportedMethodNames /* methodNames */, capabilities /* capabilities */,
                            new String[0] /* preferredRelatedApplicationIds */));
                    callback.onAllPaymentAppsCreated();
                });
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoSupportedPaymentMethods()
            throws InterruptedException, ExecutionException, TimeoutException {
        installMockServiceWorkerPaymentApp(new String[0],
                new ServiceWorkerPaymentApp.Capabilities[0], "BobPay", new ColorDrawable());

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.openPageAndClickBuyAndWait(mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"show() rejected", "The payment method", "not supported"});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testHasSupportedPaymentMethods()
            throws InterruptedException, ExecutionException, TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com"};
        installMockServiceWorkerPaymentApp(supportedMethodNames,
                new ServiceWorkerPaymentApp.Capabilities[0], "BobPay", new ColorDrawable());

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoCapabilities()
            throws InterruptedException, ExecutionException, TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com", "basic-card"};
        ServiceWorkerPaymentApp.Capabilities[] capabilities = {};
        installMockServiceWorkerPaymentApp(
                supportedMethodNames, capabilities, "BobPay", new ColorDrawable());

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The Bob Pay modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_all_cards_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_credit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_debit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_credit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testHasVisaCreditCapabilities()
            throws InterruptedException, ExecutionException, TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com", "basic-card"};
        int[] networks = {BasicCardNetwork.VISA};
        int[] types = {BasicCardType.CREDIT};
        ServiceWorkerPaymentApp.Capabilities[] capabilities = {
                new ServiceWorkerPaymentApp.Capabilities(networks, types)};
        installMockServiceWorkerPaymentApp(
                supportedMethodNames, capabilities, "BobPay", new ColorDrawable());

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_all_cards_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_credit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_debit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_credit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testHasMastercardCreditCapabilities()
            throws InterruptedException, ExecutionException, TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com", "basic-card"};
        int[] networks = {BasicCardNetwork.MASTERCARD};
        int[] types = {BasicCardType.CREDIT};
        ServiceWorkerPaymentApp.Capabilities[] capabilities = {
                new ServiceWorkerPaymentApp.Capabilities(networks, types)};
        installMockServiceWorkerPaymentApp(
                supportedMethodNames, capabilities, "BobPay", new ColorDrawable());

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_all_cards_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_credit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_debit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_credit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testHasVisaCreditAndDebitCapabilities()
            throws InterruptedException, ExecutionException, TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com", "basic-card"};
        int[] networks = {BasicCardNetwork.VISA};
        int[] types = {BasicCardType.CREDIT, BasicCardType.DEBIT};
        ServiceWorkerPaymentApp.Capabilities[] capabilities = {
                new ServiceWorkerPaymentApp.Capabilities(networks, types)};
        installMockServiceWorkerPaymentApp(
                supportedMethodNames, capabilities, "BobPay", new ColorDrawable());

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_all_cards_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_credit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_debit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_credit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testDoNotCallCanMakePayment()
            throws InterruptedException, ExecutionException, TimeoutException {
        String[] supportedMethodNames = {"basic-card"};
        installMockServiceWorkerPaymentApp(supportedMethodNames,
                new ServiceWorkerPaymentApp.Capabilities[0], "BobPay", new ColorDrawable());

        // Sets setCanMakePaymentForTesting(false) to return false for CanMakePayment since there is
        // no real sw payment app, so if CanMakePayment is called then no payment instruments will
        // be available, otherwise CanMakePayment is not called.
        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(false);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCallCanMakePayment()
            throws InterruptedException, ExecutionException, TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com", "basic-card"};
        installMockServiceWorkerPaymentApp(supportedMethodNames,
                new ServiceWorkerPaymentApp.Capabilities[0], "BobPay", new ColorDrawable());

        // Sets setCanMakePaymentForTesting(false) to return false for CanMakePayment since there is
        // no real sw payment app, so if CanMakePayment is called then no payment instruments will
        // be available, otherwise CanMakePayment is not called.
        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(false);

        mPaymentRequestTestRule.openPageAndClickBuyAndWait(mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"show() rejected", "The payment method", "not supported"});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanPreselect()
            throws InterruptedException, ExecutionException, TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com"};
        installMockServiceWorkerPaymentApp(supportedMethodNames,
                new ServiceWorkerPaymentApp.Capabilities[0], "BobPay", new ColorDrawable());

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertNotNull(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanNotPreselectWithoutName()
            throws InterruptedException, ExecutionException, TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com"};
        installMockServiceWorkerPaymentApp(supportedMethodNames,
                new ServiceWorkerPaymentApp.Capabilities[0], null, new ColorDrawable());

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertNull(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanNotPreselectWithoutIcon()
            throws InterruptedException, ExecutionException, TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com"};
        installMockServiceWorkerPaymentApp(
                supportedMethodNames, new ServiceWorkerPaymentApp.Capabilities[0], "BobPay", null);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertNull(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanNotPreselectWithoutNameAndIcon()
            throws InterruptedException, ExecutionException, TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com"};
        installMockServiceWorkerPaymentApp(
                supportedMethodNames, new ServiceWorkerPaymentApp.Capabilities[0], null, null);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertNull(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel());
    }
}
