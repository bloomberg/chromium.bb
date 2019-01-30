// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import android.support.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.autofill.PersonalDataManager;

/** Delegate for the Payment Request UI. */
@JNINamespace("autofill_assistant")
class AssistantPaymentRequestDelegate {
    private long mNativeAssistantOverlayDelegate;

    @CalledByNative
    private static AssistantPaymentRequestDelegate create(
            long nativeAssistantPaymentRequestDelegate) {
        return new AssistantPaymentRequestDelegate(nativeAssistantPaymentRequestDelegate);
    }

    private AssistantPaymentRequestDelegate(long nativeAssistantPaymentRequestDelegate) {
        mNativeAssistantOverlayDelegate = nativeAssistantPaymentRequestDelegate;
    }

    public void onPaymentInformationSelected(
            AutofillAssistantPaymentRequest.SelectedPaymentInformation selectedInformation) {
        if (mNativeAssistantOverlayDelegate != 0) {
            nativeOnGetPaymentInformation(mNativeAssistantOverlayDelegate,
                    selectedInformation.succeed, selectedInformation.card,
                    selectedInformation.address, selectedInformation.payerName,
                    selectedInformation.payerPhone, selectedInformation.payerEmail,
                    selectedInformation.isTermsAndConditionsAccepted);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantOverlayDelegate = 0;
    }

    private native void nativeOnGetPaymentInformation(long nativeAssistantPaymentRequestDelegate,
            boolean succeed, @Nullable PersonalDataManager.CreditCard card,
            @Nullable PersonalDataManager.AutofillProfile address, @Nullable String payerName,
            @Nullable String payerPhone, @Nullable String payerEmail,
            boolean isTermsAndConditionsAccepted);
}
