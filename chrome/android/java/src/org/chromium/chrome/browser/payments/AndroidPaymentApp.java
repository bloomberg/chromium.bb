// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.app.Activity;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Handler;
import android.util.JsonWriter;

import org.chromium.chrome.R;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.ui.base.WindowAndroid;

import java.io.IOException;
import java.io.StringWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** The point of interaction with a locally installed 3rd party native Android payment app. */
public class AndroidPaymentApp extends PaymentInstrument implements PaymentApp,
        WindowAndroid.IntentCallback {
    /** The action name for the Pay Intent. */
    public static final String ACTION_PAY = "org.chromium.intent.action.PAY";

    private static final String EXTRA_METHOD_NAME = "methodName";
    private static final String EXTRA_DATA = "data";
    private static final String EXTRA_ORIGIN = "origin";
    private static final String EXTRA_DETAILS = "details";
    private static final String EXTRA_INSTRUMENT_DETAILS = "instrumentDetails";
    private static final String EMPTY_JSON_DATA = "{}";

    private final Handler mHandler;
    private final WebContents mWebContents;
    private final Intent mPayIntent;
    private final Set<String> mMethodNames;
    private String mIsReadyToPayService;
    private InstrumentDetailsCallback mInstrumentDetailsCallback;

    /**
     * Builds the point of interaction with a locally installed 3rd party native Android payment
     * app.
     *
     * @param webContents The web contents.
     * @param packageName The name of the package of the payment app.
     * @param activity    The name of the payment activity in the payment app.
     * @param label       The UI label to use for the payment app.
     * @param icon        The icon to use in UI for the payment app.
     */
    public AndroidPaymentApp(WebContents webContents, String packageName, String activity,
            String label, Drawable icon) {
        super(packageName, label, null, icon);
        mHandler = new Handler();
        mWebContents = webContents;
        mPayIntent = new Intent();
        mPayIntent.setClassName(packageName, activity);
        mPayIntent.setAction(ACTION_PAY);
        mMethodNames = new HashSet<>();
    }

    /** @param methodName A payment method that this app supports, e.g., "https://bobpay.com". */
    public void addMethodName(String methodName) {
        mMethodNames.add(methodName);
    }

    /** @param service The name of the "is ready to pay" service in the payment app. */
    public void setIsReadyToPayService(String service) {
        mIsReadyToPayService = service;
    }

    @Override
    public void getInstruments(Map<String, PaymentMethodData> methodData, String origin,
            final InstrumentsCallback callback) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                List<PaymentInstrument> instruments = new ArrayList<>();
                instruments.add(AndroidPaymentApp.this);
                callback.onInstrumentsReady(AndroidPaymentApp.this, instruments);
            }
        });
    }

    @Override
    public boolean supportsMethodsAndData(Map<String, PaymentMethodData> methodsAndData) {
        assert methodsAndData != null;
        Set<String> methodNames = new HashSet<>(methodsAndData.keySet());
        methodNames.retainAll(getAppMethodNames());
        return !methodNames.isEmpty();
    }

    @Override
    public String getAppIdentifier() {
        return getIdentifier();
    }

    @Override
    public Set<String> getAppMethodNames() {
        return Collections.unmodifiableSet(mMethodNames);
    }

    @Override
    public Set<String> getInstrumentMethodNames() {
        return getAppMethodNames();
    }

    @Override
    public void invokePaymentApp(String merchantName, String origin, PaymentItem total,
            List<PaymentItem> cart, Map<String, PaymentMethodData> methodDataMap,
            InstrumentDetailsCallback callback) {
        assert !mMethodNames.isEmpty();
        Bundle extras = new Bundle();
        extras.putString(EXTRA_ORIGIN, origin);

        String methodName = mMethodNames.iterator().next();
        extras.putString(EXTRA_METHOD_NAME, methodName);

        PaymentMethodData methodData = methodDataMap.get(methodName);
        extras.putString(
                EXTRA_DATA, methodData == null ? EMPTY_JSON_DATA : methodData.stringifiedData);

        String details = serializeDetails(total, cart);
        extras.putString(EXTRA_DETAILS, details == null ? EMPTY_JSON_DATA : details);
        mPayIntent.putExtras(extras);

        mInstrumentDetailsCallback = callback;

        ContentViewCore contentView = ContentViewCore.fromWebContents(mWebContents);
        if (contentView == null) {
            notifyError();
            return;
        }

        WindowAndroid window = contentView.getWindowAndroid();
        if (window == null) {
            notifyError();
            return;
        }

        if (!window.showIntent(mPayIntent, this, R.string.payments_android_app_error)) {
            notifyError();
        }
    }

    private void notifyError() {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                mInstrumentDetailsCallback.onInstrumentDetailsError();
            }
        });
    }

    private static String serializeDetails(PaymentItem total, List<PaymentItem> cart) {
        StringWriter stringWriter = new StringWriter();
        JsonWriter json = new JsonWriter(stringWriter);
        try {
            // details {{{
            json.beginObject();

            // total {{{
            json.name("total");
            serializePaymentItem(json, total);
            // }}} total

            // displayitems {{{
            if (cart != null) {
                json.name("displayItems").beginArray();
                for (int i = 0; i < cart.size(); i++) {
                    serializePaymentItem(json, cart.get(i));
                }
                json.endArray();
            }
            // }}} displayItems

            json.endObject();
            // }}} details
        } catch (IOException e) {
            return null;
        }

        return stringWriter.toString();
    }

    private static void serializePaymentItem(JsonWriter json, PaymentItem item) throws IOException {
        // item {{{
        json.beginObject();
        json.name("label").value(item.label);

        // amount {{{
        json.name("amount").beginObject();
        json.name("currency").value(item.amount.currency);
        json.name("value").value(item.amount.value);
        json.endObject();
        // }}} amount

        json.endObject();
        // }}} item
    }

    @Override
    public void onIntentCompleted(WindowAndroid window, int resultCode, Intent data) {
        window.removeIntentCallback(this);
        if (data == null || data.getExtras() == null || resultCode != Activity.RESULT_OK) {
            mInstrumentDetailsCallback.onInstrumentDetailsError();
        } else {
            mInstrumentDetailsCallback.onInstrumentDetailsReady(
                    data.getExtras().getString(EXTRA_METHOD_NAME),
                    data.getExtras().getString(EXTRA_INSTRUMENT_DETAILS));
        }
        mInstrumentDetailsCallback = null;
    }

    @Override
    public void dismissInstrument() {}
}
