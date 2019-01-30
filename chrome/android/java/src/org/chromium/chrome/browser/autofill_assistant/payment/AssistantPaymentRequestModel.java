// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * State for the header of the Autofill Assistant.
 */
@JNINamespace("autofill_assistant")
public class AssistantPaymentRequestModel extends PropertyModel {
    static final WritableObjectPropertyKey<AssistantPaymentRequestDelegate> DELEGATE =
            new WritableObjectPropertyKey<>();

    /** The payment request options. Set to null to hide the payment request UI. */
    public static final WritableObjectPropertyKey<AssistantPaymentRequestOptions> OPTIONS =
            new WritableObjectPropertyKey<>();

    public AssistantPaymentRequestModel() {
        super(DELEGATE, OPTIONS);
    }

    @CalledByNative
    private void setOptions(String defaultEmail, boolean requestShipping, boolean requestPayerName,
            boolean requestPayerPhone, boolean requestPayerEmail,
            String[] supportedBasicCardNetworks) {
        set(OPTIONS,
                new AssistantPaymentRequestOptions(requestPayerName, requestPayerEmail,
                        requestPayerPhone, requestShipping, supportedBasicCardNetworks,
                        defaultEmail));
    }

    @CalledByNative
    private void clearOptions() {
        set(OPTIONS, null);
    }

    @CalledByNative
    private void setDelegate(AssistantPaymentRequestDelegate delegate) {
        set(DELEGATE, delegate);
    }
}
