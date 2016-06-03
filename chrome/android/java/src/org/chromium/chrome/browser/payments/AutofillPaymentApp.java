// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.os.Handler;

import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.content_public.browser.WebContents;
import org.chromium.mojom.payments.PaymentItem;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Provides access to locally stored user credit cards.
 */
public class AutofillPaymentApp implements PaymentApp {
    private final WebContents mWebContents;

    /**
     * Builds a payment app backed by autofill cards.
     *
     * @param webContents The web contents where PaymentRequest was invoked.
     */
    public AutofillPaymentApp(WebContents webContents) {
        mWebContents = webContents;
    }

    @Override
    public void getInstruments(List<PaymentItem> unusedItems, final InstrumentsCallback callback) {
        List<CreditCard> cards = PersonalDataManager.getInstance().getCreditCards();
        final List<PaymentInstrument> instruments = new ArrayList<>(cards.size());
        for (int i = 0; i < cards.size(); i++) {
            instruments.add(new AutofillPaymentInstrument(mWebContents, cards.get(i)));
        }
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                callback.onInstrumentsReady(AutofillPaymentApp.this, instruments);
            }
        });
    }

    @Override
    public Set<String> getSupportedMethodNames() {
        // https://w3c.github.io/browser-payment-api/specs/basic-card-payment.html#method-id
        // The spec also includes more detailed card types, e.g., "visa/credit" and "visa/debit".
        // Autofill does not distinguish between these types of cards, so they are not in the list
        // of supported method names.
        Set<String> methods = new HashSet<String>();

        methods.add("visa");
        methods.add("mastercard");
        methods.add("amex");
        methods.add("discover");
        methods.add("diners");
        methods.add("jcb");
        methods.add("unionpay");

        // The spec does not include "generic" card types. That's the type of card for which
        // Chrome cannot determine the type.
        methods.add("generic");

        return methods;
    }

    @Override
    public String getIdentifier() {
        return "Chrome_Autofill_Payment_App";
    }
}
