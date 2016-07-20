// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.os.Handler;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.payments.PaymentAppFactory.PaymentAppFactoryAddition;
import org.chromium.chrome.browser.payments.ui.PaymentOption;
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
    private static final int IMMEDIATE_RESPONSE = 0;
    private static final int DELAYED_RESPONSE = 1;

    public PaymentRequestPaymentAppTest() {
        super("payment_request_bobpay_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {}

    /** If no payment methods are supported, reject the show() promise before showing any UI. */
    @MediumTest
    public void testNoSupportedPaymentMethods()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mShowFailed);
        expectResultContains(
                new String[] {"show() rejected", "The payment method is not supported"});
    }

    /**
     * If Bob Pay is supported and installed, user should be able to pay with it. Here Bob Pay
     * responds to Chrome immediately.
     */
    @MediumTest
    public void testPayViaFastBobPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        installBobPay(IMMEDIATE_RESPONSE);
        triggerUIAndWait(mReadyToPay);
        clickAndWait(R.id.button_primary, mDismissed);
        expectResultContains(new String[] {"https://bobpay.com", "\"transaction\"", "1337"});
    }

    /**
     * If Bob Pay is supported and installed, user should be able to pay with it. Here Bob Pay
     * responds to Chrome after a slight delay.
     */
    @MediumTest
    public void testPayViaSlowBobPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        installBobPay(DELAYED_RESPONSE);
        triggerUIAndWait(mReadyToPay);
        clickAndWait(R.id.button_primary, mDismissed);
        expectResultContains(new String[] {"https://bobpay.com", "\"transaction\"", "1337"});
    }

    /**
     * Installs Bob Pay as a payment app in Chrome.
     *
     * @param responseSpeed How quickly Bob Pay will respond to "get instruments" query. Either
     *                      IMMEDIATE_RESPONSE or DELAYED_RESPONSE.
     */
    private void installBobPay(final int responseSpeed) {
        PaymentAppFactory.setAdditionalFactory(new PaymentAppFactoryAddition() {
            @Override
            public List<PaymentApp> create(WebContents webContents) {
                List<PaymentApp> additionalApps = new ArrayList<>();
                additionalApps.add(new PaymentApp() {
                    private static final String METHOD_NAME = "https://bobpay.com";

                    @Override
                    public void getInstruments(
                            JSONObject details, final InstrumentsCallback instrumentsCallback) {
                        final List<PaymentInstrument> instruments = new ArrayList<>();
                        instruments.add(new PaymentInstrument(
                                METHOD_NAME, "Bob Pay", null, PaymentOption.NO_ICON) {
                            @Override
                            public String getMethodName() {
                                return METHOD_NAME;
                            }

                            @Override
                            public void getDetails(String merchantName, String origin,
                                    PaymentItem total, List<PaymentItem> cart, JSONObject details,
                                    DetailsCallback detailsCallback) {
                                detailsCallback.onInstrumentDetailsReady(
                                        METHOD_NAME, "{\"transaction\": 1337}");
                            }

                            @Override
                            public void dismiss() {}
                        });

                        final PaymentApp app = this;
                        Runnable instrumentsReady = new Runnable() {
                            @Override
                            public void run() {
                                ThreadUtils.assertOnUiThread();
                                instrumentsCallback.onInstrumentsReady(app, instruments);
                            }
                        };
                        if (responseSpeed == IMMEDIATE_RESPONSE) {
                            instrumentsReady.run();
                        } else {
                            new Handler().postDelayed(instrumentsReady, 100);
                        }
                    }

                    @Override
                    public Set<String> getSupportedMethodNames() {
                        Set<String> methodNames = new HashSet<>();
                        methodNames.add(METHOD_NAME);
                        return methodNames;
                    }

                    @Override
                    public String getIdentifier() {
                        return METHOD_NAME;
                    }
                });
                return additionalApps;
            }
        });
    }
}
