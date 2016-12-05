// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.graphics.drawable.Drawable;

import org.chromium.base.annotations.SuppressFBWarnings;

import java.util.ArrayList;
import java.util.List;

/**
 * Native bridge for interacting with service worker based payment apps.
 */
// TODO(tommyt): crbug.com/669876. Remove these suppressions when we actually
// start using all of the functionality in this class.
@SuppressFBWarnings({
        "UWF_UNWRITTEN_PUBLIC_OR_PROTECTED_FIELD",
        "UUF_UNUSED_PUBLIC_OR_PROTECTED_FIELD"})
public class ServiceWorkerPaymentAppBridge {
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
     * Get a list of all the installed app manifests.
     */
    public List<Manifest> getAllAppManifests() {
        // TODO(tommyt): crbug.com/669876. Implement this function.
        return new ArrayList<Manifest>();
    }
}
