// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Context;
import android.os.Handler;
import android.text.TextUtils;
import android.util.JsonWriter;

import org.json.JSONObject;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.FullCardRequestDelegate;
import org.chromium.chrome.browser.autofill.PersonalDataManager.NormalizedAddressRequestDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentItem;

import java.io.IOException;
import java.io.StringWriter;
import java.util.List;

import javax.annotation.Nullable;

/**
 * The locally stored credit card payment instrument.
 */
public class AutofillPaymentInstrument extends PaymentInstrument
        implements FullCardRequestDelegate, NormalizedAddressRequestDelegate {
    private final Context mContext;
    private final WebContents mWebContents;
    private CreditCard mCard;
    private boolean mIsComplete;
    private String mSecurityCode;
    @Nullable private AutofillProfile mBillingAddress;
    @Nullable private InstrumentDetailsCallback mCallback;
    private boolean mIsWaitingForBillingNormalization;
    private boolean mIsWaitingForFullCardDetails;

    /**
     * Builds a payment instrument for the given credit card.
     *
     * @param context        The application context.
     * @param webContents    The web contents where PaymentRequest was invoked.
     * @param card           The autofill card that can be used for payment.
     * @param billingAddress The billing address for the card.
     */
    public AutofillPaymentInstrument(Context context, WebContents webContents, CreditCard card,
            @Nullable AutofillProfile billingAddress) {
        super(card.getGUID(), card.getObfuscatedNumber(), card.getName(),
                card.getIssuerIconDrawableId() == 0
                ? null
                : ApiCompatibilityUtils.getDrawable(
                        context.getResources(), card.getIssuerIconDrawableId()));
        mContext = context;
        mWebContents = webContents;
        mCard = card;
        mIsComplete = false;
        mBillingAddress = billingAddress;
    }

    @Override
    public String getInstrumentMethodName() {
        return mCard.getBasicCardPaymentType();
    }

    @Override
    public void getInstrumentDetails(String unusedMerchantName, String unusedOrigin,
            PaymentItem unusedTotal, List<PaymentItem> unusedCart, JSONObject unusedDetails,
            InstrumentDetailsCallback callback) {
        // The billing address should never be null for a credit card at this point.
        assert mBillingAddress != null;
        assert mIsComplete;
        assert mCallback == null;
        mCallback = callback;

        mIsWaitingForBillingNormalization = true;
        mIsWaitingForFullCardDetails = true;

        // Start the billing address normalization.
        PersonalDataManager.getInstance().normalizeAddress(
                mBillingAddress.getGUID(), AutofillAddress.getCountryCode(mBillingAddress), this);

        // Start to get the full card details.
        PersonalDataManager.getInstance().getFullCard(mWebContents, mCard, this);
    }

    @Override
    public void onFullCardDetails(CreditCard updatedCard, String cvc) {
        // Keep the cvc for after the normalization.
        mSecurityCode = cvc;

        // Update the card's expiration date.
        mCard.setMonth(updatedCard.getMonth());
        mCard.setYear(updatedCard.getYear());

        mIsWaitingForFullCardDetails = false;

        // Show the loading UI while the address gets normalized.
        mCallback.loadingInstrumentDetails();

        // Wait for the billing address normalization before sending the instrument details.
        if (mIsWaitingForBillingNormalization) {
            // If the normalization is not completed yet, Start a timer to cancel it if it takes too
            // long.
            new Handler().postDelayed(new Runnable() {
                @Override
                public void run() {
                    onAddressNormalized(null);
                }
            }, PersonalDataManager.getInstance().getNormalizationTimeoutMS());

            return;
        } else {
            sendIntrumentDetails();
        }
    }

    @Override
    public void onAddressNormalized(AutofillProfile profile) {
        if (!mIsWaitingForBillingNormalization) return;
        mIsWaitingForBillingNormalization = false;

        // If the normalization finished first, use the normalized address.
        if (profile != null) mBillingAddress = profile;

        // Wait for the full card details before sending the instrument details.
        if (!mIsWaitingForFullCardDetails) sendIntrumentDetails();
    }

    /**
     * Stringify the card details and send the resulting string and the method name to the
     * registered callback.
     */
    private void sendIntrumentDetails() {
        StringWriter stringWriter = new StringWriter();
        JsonWriter json = new JsonWriter(stringWriter);
        try {
            json.beginObject();

            json.name("cardholderName").value(mCard.getName());
            json.name("cardNumber").value(mCard.getNumber());
            json.name("expiryMonth").value(mCard.getMonth());
            json.name("expiryYear").value(mCard.getYear());
            json.name("cardSecurityCode").value(mSecurityCode);

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
            json.name("phone").value(ensureNotNull(mBillingAddress.getPhoneNumber()));

            json.endObject();

            json.endObject();
        } catch (IOException e) {
            onFullCardError();
            return;
        } finally {
            mSecurityCode = "";
        }

        mCallback.onInstrumentDetailsReady(
                mCard.getBasicCardPaymentType(), stringWriter.toString());
    }

    private static String ensureNotNull(@Nullable String value) {
        return value == null ? "" : value;
    }

    @Override
    public void onFullCardError() {
        mCallback.onInstrumentDetailsError();
        mCallback = null;
    }

    @Override
    public void dismissInstrument() {}

    /** @return Whether the card is complete and ready to be sent to the merchant as-is. */
    public boolean isComplete() {
        return mIsComplete;
    }

    /** Marks this card complete and ready to be sent to the merchant without editing first. */
    public void setIsComplete() {
        mIsComplete = true;
    }

    /**
     * Updates the instrument and marks it "complete." Called after the user has edited this
     * instrument.
     *
     * @param card           The new credit card to use. The GUID should not change.
     * @param billingAddress The billing address for the card. The GUID should match the billing
     *                       address ID of the new card to use.
     */
    public void completeInstrument(CreditCard card, AutofillProfile billingAddress) {
        assert card != null;
        assert billingAddress != null;
        assert card.getBillingAddressId() != null;
        assert card.getBillingAddressId().equals(billingAddress.getGUID());
        assert card.getIssuerIconDrawableId() != 0;

        mCard = card;
        mBillingAddress = billingAddress;
        mIsComplete = true;
        updateIdentifierLabelsAndIcon(card.getGUID(), card.getObfuscatedNumber(), card.getName(),
                null, ApiCompatibilityUtils.getDrawable(
                              mContext.getResources(), card.getIssuerIconDrawableId()));
    }

    /** @return The credit card represented by this payment instrument. */
    public CreditCard getCard() {
        return mCard;
    }

    /** @return The billing address associated with this credit card. */
    public AutofillProfile getBillingAddress() {
        return mBillingAddress;
    }
}