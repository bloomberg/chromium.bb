// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Context;
import android.os.Handler;
import android.text.TextUtils;

import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.BasicCardNetwork;
import org.chromium.payments.mojom.BasicCardType;
import org.chromium.payments.mojom.PaymentMethodData;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Provides access to locally stored user credit cards.
 */
public class AutofillPaymentApp implements PaymentApp {
    /** The method name for any type of credit card. */
    public static final String BASIC_CARD_METHOD_NAME = "basic-card";

    private final Context mContext;
    private final WebContents mWebContents;

    /**
     * Builds a payment app backed by autofill cards.
     *
     * @param context     The context.
     * @param webContents The web contents where PaymentRequest was invoked.
     */
    public AutofillPaymentApp(Context context, WebContents webContents) {
        mContext = context;
        mWebContents = webContents;
    }

    @Override
    public void getInstruments(Map<String, PaymentMethodData> methodDataMap, String unusedOrigin,
            final InstrumentsCallback callback) {
        PersonalDataManager pdm = PersonalDataManager.getInstance();
        List<CreditCard> cards = pdm.getCreditCardsToSuggest();
        final List<PaymentInstrument> instruments = new ArrayList<>(cards.size());

        Set<String> basicCardSupportedNetworks =
                convertBasicCardToNetworks(methodDataMap.get(BASIC_CARD_METHOD_NAME));

        for (int i = 0; i < cards.size(); i++) {
            CreditCard card = cards.get(i);
            AutofillProfile billingAddress = TextUtils.isEmpty(card.getBillingAddressId())
                    ? null : pdm.getProfile(card.getBillingAddressId());

            if (billingAddress != null
                    && AutofillAddress.checkAddressCompletionStatus(billingAddress)
                            != AutofillAddress.COMPLETE) {
                billingAddress = null;
            }

            if (billingAddress == null) card.setBillingAddressId(null);

            String methodName = null;
            if (basicCardSupportedNetworks != null
                    && basicCardSupportedNetworks.contains(card.getBasicCardPaymentType())) {
                methodName = BASIC_CARD_METHOD_NAME;
            } else if (methodDataMap.containsKey(card.getBasicCardPaymentType())) {
                methodName = card.getBasicCardPaymentType();
            }

            if (methodName != null) {
                instruments.add(new AutofillPaymentInstrument(mContext, mWebContents, card,
                        billingAddress, methodName));
            }
        }

        new Handler().post(new Runnable() {
            @Override
            public void run() {
                callback.onInstrumentsReady(AutofillPaymentApp.this, instruments);
            }
        });
    }

    /** @return A set of card networks (e.g., "visa", "amex") accepted by "basic-card" method. */
    public static Set<String> convertBasicCardToNetworks(PaymentMethodData data) {
        // No basic card support.
        if (data == null) return null;

        boolean supportedTypesEmpty =
                data.supportedTypes == null || data.supportedTypes.length == 0;
        boolean supportedNetworksEmpty =
                data.supportedNetworks == null || data.supportedNetworks.length == 0;

        // All basic cards are supported.
        Map<Integer, String> networks = getNetworks();
        if (supportedTypesEmpty && supportedNetworksEmpty) return new HashSet<>(networks.values());

        // https://w3c.github.io/webpayments-methods-card/#basiccardrequest defines 3 types of
        // cards: "credit", "debit", and "prepaid". Specifying a non-empty subset of these types
        // requires filtering by type, which Chrome cannot yet do.
        // TODO(rouslan): Distinguish card types. http://crbug.com/602665
        if (!supportedTypesEmpty) {
            if (data.supportedTypes.length != 3) return null;
            Set<Integer> supportedTypes = new HashSet<>();
            for (int i = 0; i < data.supportedTypes.length; i++) {
                supportedTypes.add(data.supportedTypes[i]);
            }
            if (!supportedTypes.contains(BasicCardType.CREDIT)) return null;
            if (!supportedTypes.contains(BasicCardType.DEBIT)) return null;
            if (!supportedTypes.contains(BasicCardType.PREPAID)) return null;
        }

        // All basic cards are supported.
        if (supportedNetworksEmpty) return new HashSet<>(networks.values());

        Set<String> result = new HashSet<>();
        for (int i = 0; i < data.supportedNetworks.length; i++) {
            String network = networks.get(data.supportedNetworks[i]);
            if (network != null) result.add(network);
        }
        return result;
    }

    private static Map<Integer, String> getNetworks() {
        Map<Integer, String> networks = new HashMap<>();
        networks.put(BasicCardNetwork.AMEX, "amex");
        networks.put(BasicCardNetwork.DINERS, "diners");
        networks.put(BasicCardNetwork.DISCOVER, "discover");
        networks.put(BasicCardNetwork.JCB, "jcb");
        networks.put(BasicCardNetwork.MASTERCARD, "mastercard");
        networks.put(BasicCardNetwork.MIR, "mir");
        networks.put(BasicCardNetwork.UNIONPAY, "unionpay");
        networks.put(BasicCardNetwork.VISA, "visa");
        return networks;
    }

    @Override
    public Set<String> getAppMethodNames() {
        Set<String> methods = new HashSet<>(getNetworks().values());
        methods.add(BASIC_CARD_METHOD_NAME);
        return methods;
    }

    @Override
    public boolean supportsMethodsAndData(Map<String, PaymentMethodData> methodDataMap) {
        assert methodDataMap != null;

        PaymentMethodData basicCardData = methodDataMap.get(BASIC_CARD_METHOD_NAME);
        if (basicCardData != null) {
            Set<String> basicCardNetworks = convertBasicCardToNetworks(basicCardData);
            if (basicCardNetworks != null && !basicCardNetworks.isEmpty()) return true;
        }

        Set<String> methodNames = new HashSet<>(methodDataMap.keySet());
        methodNames.retainAll(getNetworks().values());
        return !methodNames.isEmpty();
    }

    @Override
    public String getAppIdentifier() {
        return "Chrome_Autofill_Payment_App";
    }
}
