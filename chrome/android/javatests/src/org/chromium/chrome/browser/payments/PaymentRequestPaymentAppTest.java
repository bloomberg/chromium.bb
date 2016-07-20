// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.os.Handler;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.payments.PaymentAppFactory.PaymentAppFactoryAddition;
import org.chromium.content_public.browser.WebContents;
import org.chromium.mojom.payments.PaymentItem;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests payment via Bob Pay.
 */
public class PaymentRequestPaymentAppTest extends PaymentRequestTestBase {
    private static final int NO_INSTRUMENTS = 0;
    private static final int HAVE_INSTRUMENTS = 1;

    private static final int IMMEDIATE_RESPONSE = 0;
    private static final int DELAYED_RESPONSE = 1;

    public PaymentRequestPaymentAppTest() {
        super("payment_request_bobpay_test.html");
    }

    @Override
    public void onMainActivityStarted() throws InterruptedException, ExecutionException,
            TimeoutException {}

    /** If no payment methods are supported, reject the show() promise. */
    @MediumTest
    public void testNoSupportedPaymentMethods() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(mShowFailed);
        expectResultContains(
                new String[]{"show() rejected", "The payment method is not supported"});
    }

    /**
     * If Bob Pay does not have any instruments, reject the show() promise. Here Bob Pay responds to
     * Chrome immediately.
     */
    @MediumTest
    public void testNoInstrumentsInFastBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installBobPay(NO_INSTRUMENTS, IMMEDIATE_RESPONSE);
        triggerUIAndWait(mShowFailed);
        expectResultContains(
                new String[]{"show() rejected", "The payment method is not supported"});
    }

    /**
     * If Bob Pay does not have any instruments, reject the show() promise. Here Bob Pay responds to
     * Chrome after a slight delay.
     */
    @MediumTest
    public void testNoInstrumentsInSlowBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installBobPay(NO_INSTRUMENTS, DELAYED_RESPONSE);
        triggerUIAndWait(mShowFailed);
        expectResultContains(
                new String[]{"show() rejected", "The payment method is not supported"});
    }

    /**
     * If Bob Pay is supported and installed, user should be able to pay with it. Here Bob Pay
     * responds to Chrome immediately.
     */
    @MediumTest
    public void testPayViaFastBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installBobPay(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        triggerUIAndWait(mReadyToPay);
        clickAndWait(R.id.button_primary, mDismissed);
        expectResultContains(new String[]{"https://bobpay.com", "\"transaction\"", "1337"});
    }

    /**
     * If Bob Pay is supported and installed, user should be able to pay with it. Here Bob Pay
     * responds to Chrome after a slight delay.
     */
    @MediumTest
    public void testPayViaSlowBobPay() throws InterruptedException, ExecutionException,
            TimeoutException {
        installBobPay(HAVE_INSTRUMENTS, DELAYED_RESPONSE);
        triggerUIAndWait(mReadyToPay);
        clickAndWait(R.id.button_primary, mDismissed);
        expectResultContains(new String[]{"https://bobpay.com", "\"transaction\"", "1337"});
    }

    /**
     * Installs a payment app for testing.
     *
     * @param instrumentPresence Whether Bob Pay has any payment instruments. Either NO_INSTRUMENTS
     *                           or HAVE_INSTRUMENTS.
     * @param responseSpeed      How quickly Bob Pay will respond to "get instruments" query. Either
     *                           IMMEDIATE_RESPONSE or DELAYED_RESPONSE.
     */
    private void installBobPay(final int instrumentPresence, final int responseSpeed) {
        PaymentAppFactory.setAdditionalFactory(new PaymentAppFactoryAddition() {
            @Override
            public List<PaymentApp> create(WebContents webContents) {
                List<PaymentApp> additionalApps = new ArrayList<>();
                additionalApps.add(new BobPay(instrumentPresence, responseSpeed));
                return additionalApps;
            }
        });
    }

    /** A payment app implementation for test. */
    private static class BobPay implements PaymentApp {
        private final int mInstrumentPresence;
        private final int mResponseSpeed;

        BobPay(int instrumentPresence, int responseSpeed) {
            mInstrumentPresence = instrumentPresence;
            mResponseSpeed = responseSpeed;
        }

        @Override
        public void getInstruments(JSONObject details, final InstrumentsCallback
                instrumentsCallback) {
            final List<PaymentInstrument> instruments = new ArrayList<>();
            if (mInstrumentPresence == HAVE_INSTRUMENTS) instruments.add(new BobPayInstrument());
            Runnable instrumentsReady = new Runnable() {
                @Override
                public void run() {
                    ThreadUtils.assertOnUiThread();
                    instrumentsCallback.onInstrumentsReady(BobPay.this, instruments);
                }
            };
            if (mResponseSpeed == IMMEDIATE_RESPONSE) {
                instrumentsReady.run();
            } else {
                new Handler().postDelayed(instrumentsReady, 100);
            }
        }

        @Override
        public Set<String> getSupportedMethodNames() {
            Set<String> methodNames = new HashSet<>();
            methodNames.add("https://bobpay.com");
            return methodNames;
        }

        @Override
        public String getIdentifier() {
            return "https://bobpay.com";
        }
    }

    /** A payment instrument implementation for test. */
    private static class BobPayInstrument extends PaymentInstrument {
        BobPayInstrument() {
            super("https://bobpay.com", "Bob Pay", null, NO_ICON);
        }

        @Override
        public String getMethodName() {
            return "https://bobpay.com";
        }

        @Override
        public void getDetails(String merchantName, String origin, PaymentItem total,
                List<PaymentItem> cart, JSONObject details, DetailsCallback detailsCallback) {
            detailsCallback.onInstrumentDetailsReady(
                    "https://bobpay.com", "{\"transaction\": 1337}");
        }

        @Override
        public void dismiss() {}
    }
}
