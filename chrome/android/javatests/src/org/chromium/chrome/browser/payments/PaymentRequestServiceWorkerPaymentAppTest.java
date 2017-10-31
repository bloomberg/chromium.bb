// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

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
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.payments.mojom.BasicCardNetwork;
import org.chromium.payments.mojom.BasicCardType;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for service worker based payment apps.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
public class PaymentRequestServiceWorkerPaymentAppTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule = new PaymentRequestTestRule(
            "payment_request_bobpay_and_basic_card_with_optional_data_test.html");

    /**
     * Installs a mock service worker based payment app for testing.
     *
     * @param supportedMethodNames The supported payment methods of the mock payment app.
     * @param capabilities         The capabilities of the mocked payment app.
     */
    private void installMockServiceWorkerPaymentApp(final String[] supportedMethodNames,
            final ServiceWorkerPaymentApp.Capabilities[] capabilities) {
        PaymentAppFactory.getInstance().addAdditionalFactory(
                (webContents, methodNames, callback) -> {
                    ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);
                    callback.onPaymentAppCreated(new ServiceWorkerPaymentApp(webContents,
                            0 /* registrationId */,
                            UriUtils.parseUriFromString("https://bobpay.com") /* scope */,
                            "BobPay" /* label */, null /* sublabel*/,
                            "https://bobpay.com" /* tertiarylabel */, null /* icon */,
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
        String[] supportedMethodNames = new String[0];
        ServiceWorkerPaymentApp.Capabilities[] capabilities = {};
        installMockServiceWorkerPaymentApp(supportedMethodNames, capabilities);

        mPaymentRequestTestRule.openPageAndClickBuyAndWait(mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"show() rejected", "The payment method is not supported"});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add({"disable-features=" + ChromeFeatureList.WEB_PAYMENTS_SINGLE_APP_UI_SKIP})
    public void testHasSupportedPaymentMethods()
            throws InterruptedException, ExecutionException, TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com"};
        ServiceWorkerPaymentApp.Capabilities[] capabilities = {};
        installMockServiceWorkerPaymentApp(supportedMethodNames, capabilities);

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
        installMockServiceWorkerPaymentApp(supportedMethodNames, capabilities);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_all_cards", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_credit", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(0, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_debit", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(0, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_credit", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(0, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(0, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
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
        installMockServiceWorkerPaymentApp(supportedMethodNames, capabilities);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_all_cards", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_credit", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_debit", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(0, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_credit", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
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
        installMockServiceWorkerPaymentApp(supportedMethodNames, capabilities);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_all_cards", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_credit", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(0, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_debit", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(0, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_credit", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(0, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
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
        installMockServiceWorkerPaymentApp(supportedMethodNames, capabilities);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_all_cards", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_credit", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_debit", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_credit", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testHasBobPayAndNoCapabilities()
            throws InterruptedException, ExecutionException, TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com", "basic-card"};
        ServiceWorkerPaymentApp.Capabilities[] capabilities = {};
        installMockServiceWorkerPaymentApp(supportedMethodNames, capabilities);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_bobpay_and_visa", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
    }
}
