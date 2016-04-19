// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.content_public.browser.WebContents;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojom.payments.PaymentDetails;
import org.chromium.mojom.payments.PaymentOptions;
import org.chromium.mojom.payments.PaymentRequest;
import org.chromium.mojom.payments.PaymentRequestClient;

/**
 * Android implementation of the PaymentRequest service defined in
 * third_party/WebKit/public/platform/modules/payments/payment_request.mojom.
 */
public class PaymentRequestImpl implements PaymentRequest {
    /**
     * Builds the dialog.
     *
     * @param webContents The web contents that have invoked the PaymentRequest API.
     */
    public PaymentRequestImpl(WebContents webContents) {}

    /**
     * Called by the renderer to provide an endpoint for callbacks.
     */
    @Override
    public void setClient(PaymentRequestClient client) {
        assert client != null;
        client.onError();
    }

    /**
     * Called by the merchant website to show the payment request to the user.
     */
    @Override
    public void show(String[] supportedMethods, PaymentDetails details, PaymentOptions options,
            String stringifiedData) {}

    /**
     * Called by the merchant website to abort the payment.
     */
    @Override
    public void abort() {}

    /**
     * Called when the merchant website has processed the payment.
     */
    @Override
    public void complete(boolean success) {}

    /**
     * Called when the renderer closes the Mojo connection.
     */
    @Override
    public void close() {}

    /**
     * Called when the Mojo connection encounters an error.
     */
    @Override
    public void onConnectionError(MojoException e) {}
}
