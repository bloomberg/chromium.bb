// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.text.TextUtils;
import android.util.JsonWriter;

import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.FullCardRequestDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.mojom.payments.PaymentItem;

import org.json.JSONObject;

import java.io.IOException;
import java.io.StringWriter;
import java.util.List;

import javax.annotation.Nullable;

/**
 * The locally stored credit card payment instrument.
 */
public class AutofillPaymentInstrument
        extends PaymentInstrument implements FullCardRequestDelegate {
    private final WebContents mWebContents;
    private final CreditCard mCard;
    @Nullable private AutofillProfile mBillingAddress;
    @Nullable private DetailsCallback mCallback;

    /**
     * Builds a payment instrument for the given credit card.
     *
     * @param webContents    The web contents where PaymentRequest was invoked.
     * @param card           The autofill card that can be used for payment.
     * @param billingAddress The optional billing address for the card.
     */
    public AutofillPaymentInstrument(
            WebContents webContents, CreditCard card, @Nullable AutofillProfile billingAddress) {
        super(card.getGUID(), card.getObfuscatedNumber(), card.getName(),
                card.getIssuerIconDrawableId());
        mWebContents = webContents;
        mCard = card;
        mBillingAddress = billingAddress;
    }

    @Override
    public String getMethodName() {
        return mCard.getBasicCardPaymentType();
    }

    @Override
    public void getDetails(String unusedMerchantName, String unusedOrigin, PaymentItem unusedTotal,
            List<PaymentItem> unusedCart, JSONObject unusedDetails, DetailsCallback callback) {
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

            if (mBillingAddress != null) {
                json.name("billingAddress").beginObject();

                json.name("country").value(ensureNotNull(mBillingAddress.getCountryCode()));
                json.name("region").value(ensureNotNull(mBillingAddress.getRegion()));
                json.name("city").value(ensureNotNull(mBillingAddress.getLocality()));
                json.name("dependentLocality")
                        .value(ensureNotNull(mBillingAddress.getDependentLocality()));

                json.name("addressLine").beginArray();
                String multipleLines = ensureNotNull(mBillingAddress.getStreetAddress());
                if (!TextUtils.isEmpty(multipleLines)) {
                    String[] lines = multipleLines.split("\n");
                    for (int i = 0; i < lines.length; i++) {
                        json.value(lines[i]);
                    }
                }
                json.endArray();

                json.name("postalCode").value(ensureNotNull(mBillingAddress.getPostalCode()));
                json.name("sortingCode").value(ensureNotNull(mBillingAddress.getSortingCode()));
                json.name("languageCode").value(ensureNotNull(mBillingAddress.getLanguageCode()));
                json.name("organization").value(ensureNotNull(mBillingAddress.getCompanyName()));
                json.name("recipient").value(ensureNotNull(mBillingAddress.getFullName()));
                json.name("careOf").value("");
                json.name("phone").value(ensureNotNull(mBillingAddress.getPhoneNumber()));

                json.endObject();
            }

            json.endObject();
        } catch (IOException e) {
            mCallback.onInstrumentDetailsError();
            return;
        }

        mCallback.onInstrumentDetailsReady(card.getBasicCardPaymentType(), stringWriter.toString());
    }

    private static String ensureNotNull(@Nullable String value) {
        return value == null ? "" : value;
    }

    @Override
    public void onFullCardError() {
        mCallback.onInstrumentDetailsError();
    }

    @Override
    public void dismiss() {}
}
