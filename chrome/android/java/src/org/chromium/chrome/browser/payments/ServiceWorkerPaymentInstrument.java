// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;

import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * This instrument class represents a single payment option for a service
 * worker based payment app.
 *
 * @see org.chromium.chrome.browser.payments.ServiceWorkerPaymentApp
 *
 * @see https://w3c.github.io/webpayments-payment-apps-api/
 */
public class ServiceWorkerPaymentInstrument extends PaymentInstrument {
    private final ServiceWorkerPaymentAppBridge.Option mOption;
    private final Set<String> mMethodNames;

    /**
     * Build a service worker based payment instrument based on a single payment option
     * of an installed payment app.
     *
     * @see https://w3c.github.io/webpayments-payment-apps-api/#payment-app-options
     *
     * @param scopeUrl    The scope url of the corresponding service worker payment app.
     * @param option      A payment app option from the payment app.
     */
    public ServiceWorkerPaymentInstrument(String scopeUrl,
            ServiceWorkerPaymentAppBridge.Option option) {
        super(scopeUrl + "#" + option.id, option.label, null /* icon */, option.icon);
        mOption = option;

        mMethodNames = new HashSet<String>(option.enabledMethods);
    }

    @Override
    public Set<String> getInstrumentMethodNames() {
        return Collections.unmodifiableSet(mMethodNames);
    }

    @Override
    public void invokePaymentApp(String merchantName, String origin, PaymentItem total,
            List<PaymentItem> cart, Map<String, PaymentMethodData> methodData,
            InstrumentDetailsCallback callback) {
        // TODO(tommyt): crbug.com/669876. Implement this for use with Service Worker Payment Apps.
        callback.onInstrumentDetailsError();
    }

    @Override
    public void dismissInstrument() {}
}
