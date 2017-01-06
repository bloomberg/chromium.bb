// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentMethodData;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * Native bridge for interacting with service worker based payment apps.
 */
// TODO(tommyt): crbug.com/669876. Remove these suppressions when we actually
// start using all of the functionality in this class.
@SuppressFBWarnings({"UWF_NULL_FIELD", "URF_UNREAD_PUBLIC_OR_PROTECTED_FIELD",
        "UWF_UNWRITTEN_PUBLIC_OR_PROTECTED_FIELD", "UUF_UNUSED_PUBLIC_OR_PROTECTED_FIELD"})
public class ServiceWorkerPaymentAppBridge implements PaymentAppFactory.PaymentAppFactoryAddition {
    /**
     * This class represents a payment app manifest as defined in the Payment
     * App API specification.
     *
     * @see https://w3c.github.io/webpayments-payment-apps-api/#payment-app-manifest
     */
    public static class Manifest {
        /**
         * The registration ID of the service worker.
         *
         * This can be used to identify a service worker based payment app.
         */
        public long registrationId;
        public String label;
        public Drawable icon;
        public List<Option> options = new ArrayList<>();
    }

    /**
     * This class represents a payment option as defined in the Payment App API
     * specification.
     *
     * @see https://w3c.github.io/webpayments-payment-apps-api/#payment-app-options
     */
    public static class Option {
        public String id;
        public String label;
        public Drawable icon;
        public List<String> enabledMethods = new ArrayList<>();
    }

    @Override
    public void create(Context context, WebContents webContents, Set<String> methodNames,
            PaymentAppFactory.PaymentAppCreatedCallback callback) {
        nativeGetAllAppManifests(webContents, callback);
    }

    /**
     * Invoke a payment app with a given option and matching method data.
     *
     * @param webContents    The web contents that invoked PaymentRequest.
     * @param registrationId The service worker registration ID of the Payment App.
     * @param optionId       The ID of the PaymentOption that was selected by the user.
     * @param methodDataSet  The PaymentMethodData objects that are relevant for this payment
     * app.
     */
    public static void invokePaymentApp(WebContents webContents, long registrationId,
            String optionId, Set<PaymentMethodData> methodDataSet) {
        nativeInvokePaymentApp(webContents, registrationId, optionId,
                methodDataSet.toArray(new PaymentMethodData[methodDataSet.size()]));
    }

    @CalledByNative
    private static Manifest createManifest(long registrationId, String label, String icon) {
        Manifest manifest = new Manifest();
        manifest.registrationId = registrationId;
        manifest.label = label;
        // TODO(tommyt): crbug.com/669876. Handle icons.
        manifest.icon = null;
        return manifest;
    }

    @CalledByNative
    private static Option createAndAddOption(
            Manifest manifest, String id, String label, String icon) {
        Option option = new Option();
        option.id = id;
        option.label = label;
        // TODO(tommyt): crbug.com/669876. Handle icons.
        option.icon = null;
        manifest.options.add(option);
        return option;
    }

    @CalledByNative
    private static void addEnabledMethod(Option option, String enabledMethod) {
        option.enabledMethods.add(enabledMethod);
    }

    @CalledByNative
    private static void onGotManifest(Manifest manifest, WebContents webContents, Object callback) {
        assert callback instanceof PaymentAppFactory.PaymentAppCreatedCallback;
        ((PaymentAppFactory.PaymentAppCreatedCallback) callback)
                .onPaymentAppCreated(new ServiceWorkerPaymentApp(webContents, manifest));
    }

    @CalledByNative
    private static void onGotAllManifests(Object callback) {
        assert callback instanceof PaymentAppFactory.PaymentAppCreatedCallback;
        ((PaymentAppFactory.PaymentAppCreatedCallback) callback).onAllPaymentAppsCreated();
    }

    /*
     * TODO(tommyt): crbug.com/505554. Change the |callback| parameter below to
     * be of type PaymentAppFactory.PaymentAppCreatedCallback, once this JNI bug
     * has been resolved.
     */
    private static native void nativeGetAllAppManifests(WebContents webContents, Object callback);

    private static native void nativeInvokePaymentApp(WebContents webContents, long registrationId,
            String optionId, PaymentMethodData[] methodData);
}
