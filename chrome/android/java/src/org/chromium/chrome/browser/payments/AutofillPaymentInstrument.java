// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.util.JsonWriter;

import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.FullCardRequestDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.mojom.payments.PaymentItem;

import org.json.JSONObject;

import java.io.IOException;
import java.io.StringWriter;
import java.util.List;

/**
 * The locally stored credit card payment instrument.
 */
public class AutofillPaymentInstrument
        extends PaymentInstrument implements FullCardRequestDelegate {
    private final WebContents mWebContents;
    private final CreditCard mCard;
    private DetailsCallback mCallback;

    /**
     * Builds a payment instrument for the given credit card.
     *
     * @param webContents The web contents where PaymentRequest was invoked.
     * @param card The autofill card that can be used for payment.
     */
    public AutofillPaymentInstrument(WebContents webContents, CreditCard card) {
        super(card.getGUID(), card.getObfuscatedNumber(), card.getName(),
                card.getIssuerIconDrawableId());
        mWebContents = webContents;
        mCard = card;
    }

    @Override
    public String getMethodName() {
        return mCard.getBasicCardPaymentType();
    }

    @Override
    public void getDetails(String unusedMerchantName, String unusedOrigin,
            List<PaymentItem> unusedItems, JSONObject unusedDetails, DetailsCallback callback) {
        assert mCallback == null;
        mCallback = callback;
        PersonalDataManager.getInstance().getFullCard(mWebContents, mCard.getGUID(), this);
    }

    @Override
    public void onFullCardDetails(CreditCard card, String cvc) {
        StringWriter stringWriter = new StringWriter();
        JsonWriter json = new JsonWriter(stringWriter);
        try {
            json.beginObject();
            json.name("cardholderName").value(card.getName());
            json.name("cardNumber").value(card.getNumber());
            json.name("expiryMonth").value(card.getMonth());
            json.name("expiryYear").value(card.getYear());
            json.name("cardSecurityCode").value(cvc);
            json.endObject();
        } catch (IOException e) {
            mCallback.onInstrumentDetailsError();
            return;
        }

        mCallback.onInstrumentDetailsReady(card.getBasicCardPaymentType(), stringWriter.toString());
    }

    @Override
    public void onFullCardError() {
        mCallback.onInstrumentDetailsError();
    }

    @Override
    public void dismiss() {}
}
