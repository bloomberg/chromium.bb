// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Context;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/**
 * Builds instances of payment apps.
 */
public class PaymentAppFactory {
    /**
     * Can be used to build additional types of payment apps without Chrome knowing about their
     * types.
     */
    private static PaymentAppFactoryAddition sAdditionalFactory;

    /**
     * Native bridge that can be used to retrieve information about installed service worker
     * payment apps.
     */
    private static ServiceWorkerPaymentAppBridge sServiceWorkerPaymentAppBridge;

    /**
     * The interface for additional payment app factories.
     */
    public interface PaymentAppFactoryAddition {
        /**
         * Builds instances of payment apps.
         *
         * @param context     The application context.
         * @param webContents The web contents that invoked PaymentRequest.
         */
        List<PaymentApp> create(Context context, WebContents webContents);
    }

    /**
     * Sets the additional factory that can build instances of payment apps.
     *
     * @param additionalFactory Can build instances of payment apps.
     */
    @VisibleForTesting
    public static void setAdditionalFactory(PaymentAppFactoryAddition additionalFactory) {
        sAdditionalFactory = additionalFactory;
    }

    /**
     * Set a custom native bridge for service worker payment apps for testing.
     */
    @VisibleForTesting
    public static void setServiceWorkerPaymentAppBridgeForTest(
            ServiceWorkerPaymentAppBridge bridge) {
        sServiceWorkerPaymentAppBridge = bridge;
    }

    /**
     * Builds instances of payment apps.
     *
     * @param context     The context.
     * @param webContents The web contents where PaymentRequest was invoked.
     */
    public static List<PaymentApp> create(Context context, WebContents webContents) {
        List<PaymentApp> result = new ArrayList<>(2);
        result.add(new AutofillPaymentApp(context, webContents));

        result.addAll(createServiceWorkerPaymentApps());

        if (sAdditionalFactory != null) {
            result.addAll(
                    sAdditionalFactory.create(context, webContents));
        }
        return result;
    }

    /**
     * Build a ServiceWorkerPaymentApp instance for each installed service
     * worker based payment app.
     */
    private static List<ServiceWorkerPaymentApp> createServiceWorkerPaymentApps() {
        if (sServiceWorkerPaymentAppBridge == null) {
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.SERVICE_WORKER_PAYMENT_APPS)) {
                sServiceWorkerPaymentAppBridge = new ServiceWorkerPaymentAppBridge();
            } else {
                return new ArrayList<>();
            }
        }

        List<ServiceWorkerPaymentApp> result = new ArrayList<>();

        for (ServiceWorkerPaymentAppBridge.Manifest manifest :
                sServiceWorkerPaymentAppBridge.getAllAppManifests()) {
            result.add(new ServiceWorkerPaymentApp(manifest));
        }

        return result;
    }
}
