// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.mojo;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.payments.PaymentRequestFactory;
import org.chromium.content.browser.ServiceRegistry;
import org.chromium.mojom.payments.PaymentRequest;

/**
 * Registers mojo services exposed by Chrome in the given registry.
 */
class ChromeServiceRegistrar {
    @CalledByNative
    private static void registerRenderFrameMojoServices(
            ServiceRegistry registry, Context applicationContext) {
        assert applicationContext != null;
        registry.addService(PaymentRequest.MANAGER, new PaymentRequestFactory(applicationContext));
    }
}
