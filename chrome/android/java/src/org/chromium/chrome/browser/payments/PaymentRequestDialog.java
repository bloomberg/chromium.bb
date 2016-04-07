// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Context;

import org.chromium.mojo.system.MojoException;
import org.chromium.mojom.payments.PaymentDetails;
import org.chromium.mojom.payments.PaymentOptions;
import org.chromium.mojom.payments.PaymentRequest;
import org.chromium.mojom.payments.PaymentRequestClient;

/**
 * Android implementation of the PaymentRequest service defined in
 * third_party/WebKit/public/platform/modules/payments/payment_request.mojom.
 */
public class PaymentRequestDialog implements PaymentRequest {
    private final Context mApplicationContext;
    private PaymentRequestClient mClient;

    /**
     * Builds the dialog.
     *
     * @param applicationContext The application context.
     */
    public PaymentRequestDialog(Context applicationContext) {
        mApplicationContext = applicationContext;
    }

    @Override
    public void setClient(PaymentRequestClient client) {
        mClient = client;
    }

    @Override
    public void show(String[] supportedMethods, PaymentDetails details, PaymentOptions options,
            String stringifiedData) {
        assert mClient != null;
        mClient.onError();
    }

    @Override
    public void abort() {}

    @Override
    public void complete(boolean success) {
        assert mClient != null;
        mClient.onComplete();
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}
}
