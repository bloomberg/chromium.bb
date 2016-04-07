// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Context;

import org.chromium.content.browser.ServiceRegistry.ImplementationFactory;
import org.chromium.mojom.payments.PaymentRequest;

/**
 * Creates instances of PaymentRequest.
 */
public class PaymentRequestFactory implements ImplementationFactory<PaymentRequest> {
    private final Context mApplicationContext;

    /**
     * Builds a factory for PaymentRequest.
     *
     * @param applicationContext The application context.
     */
    public PaymentRequestFactory(Context applicationContext) {
        mApplicationContext = applicationContext;
    }

    @Override
    public PaymentRequest createImpl() {
        return new PaymentRequestDialog(mApplicationContext);
    }
}
