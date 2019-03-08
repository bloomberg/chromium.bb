// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import android.support.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.payments.AutofillContact;

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

    public void onContactInfoChanged(AutofillContact contact) {
        if (mNativeAssistantOverlayDelegate != 0) {
            String name = null;
            String phone = null;
            String email = null;

            if (contact != null) {
                name = contact.getPayerName();
                phone = contact.getPayerPhone();
                email = contact.getPayerEmail();
            }

            nativeOnContactInfoChanged(mNativeAssistantOverlayDelegate, name, phone, email);
        }
    }

    public void onShippingAddressChanged(PersonalDataManager.AutofillProfile address) {
        if (mNativeAssistantOverlayDelegate != 0) {
            nativeOnShippingAddressChanged(mNativeAssistantOverlayDelegate, address);
        }
    }

    public void onCreditCardChanged(PersonalDataManager.CreditCard card) {
        if (mNativeAssistantOverlayDelegate != 0) {
            nativeOnCreditCardChanged(mNativeAssistantOverlayDelegate, card);
        }
    }

    public void onTermsAndConditionsChanged(@AssistantTermsAndConditionsState int state) {
        if (mNativeAssistantOverlayDelegate != 0) {
            nativeOnTermsAndConditionsChanged(mNativeAssistantOverlayDelegate, state);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantOverlayDelegate = 0;
    }

    private native void nativeOnContactInfoChanged(long nativeAssistantPaymentRequestDelegate,
            @Nullable String payerName, @Nullable String payerPhone, @Nullable String payerEmail);
    private native void nativeOnShippingAddressChanged(long nativeAssistantPaymentRequestDelegate,
            @Nullable PersonalDataManager.AutofillProfile address);
    private native void nativeOnCreditCardChanged(long nativeAssistantPaymentRequestDelegate,
            @Nullable PersonalDataManager.CreditCard card);
    private native void nativeOnTermsAndConditionsChanged(
            long nativeAssistantPaymentRequestDelegate, int state);
}
