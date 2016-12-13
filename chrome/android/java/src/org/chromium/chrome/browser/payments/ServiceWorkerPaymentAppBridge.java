// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentMethodData;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

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
         * The scope url of the service worker.
         *
         * This can be used to identify a service worker based payment app.
         */
        public String scopeUrl;
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

    /**
     * Fetch all the installed service worker app manifests.
     *
     * This method is protected so that it can be overridden by tests.
     *
     * @return The installed service worker app manifests.
     */
    @VisibleForTesting
    protected List<Manifest> getAllAppManifests() {
        return nativeGetAllAppManifests();
    }

    @Override
    public void create(Context context, WebContents webContents,
            PaymentAppFactory.PaymentAppCreatedCallback callback) {
        List<Manifest> manifests = getAllAppManifests();
        for (int i = 0; i < manifests.size(); i++) {
            callback.onPaymentAppCreated(new ServiceWorkerPaymentApp(manifests.get(i)));
        }
        callback.onAllPaymentAppsCreated();
    }

    /**
     * Invoke a payment app with a given option and matching method data.
     */
    public void invokePaymentapp(
            String scopeUrl, String optionId, Map<String, PaymentMethodData> methodData) {
        nativeInvokePaymentApp(scopeUrl, optionId, methodData);
    }

    @CalledByNative
    private static List<Manifest> createManifestList() {
        return new ArrayList<Manifest>();
    }

    @CalledByNative
    private static Manifest createAndAddManifest(
            List<Manifest> manifestList, String scopeUrl, String label, String icon) {
        Manifest manifest = new Manifest();
        manifest.scopeUrl = scopeUrl;
        manifest.label = label;
        // TODO(tommyt): crbug.com/669876. Handle icons.
        manifest.icon = null;
        manifestList.add(manifest);
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

    private static native List<Manifest> nativeGetAllAppManifests();
    private static native void nativeInvokePaymentApp(
            String scopeUrl, String optionId, Object methodDataMap);
}
