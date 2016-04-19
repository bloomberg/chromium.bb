// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.content.browser.ServiceRegistry.ImplementationFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.mojom.payments.PaymentRequest;

/**
 * Creates instances of PaymentRequest.
 */
public class PaymentRequestFactory implements ImplementationFactory<PaymentRequest> {
    private final WebContents mWebContents;

    /**
     * Builds a factory for PaymentRequest.
     *
     * @param webContents The web contents that may invoke the PaymentRequest API.
     */
    public PaymentRequestFactory(WebContents webContents) {
        mWebContents = webContents;
    }

    @Override
    public PaymentRequest createImpl() {
        return new PaymentRequestImpl(mWebContents);
    }
}
