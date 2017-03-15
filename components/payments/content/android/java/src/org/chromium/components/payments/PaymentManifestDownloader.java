// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.WebContents;

import java.net.URI;

/**
 * See comment in:
 * components/payments/content/android/payment_manifest_downloader.h
 */
@JNINamespace("payments")
public class PaymentManifestDownloader {
    /** Interface for the callback to invoke when finished downloading. */
    public interface ManifestDownloadCallback {
        /**
         * Called on successful download of a payment method manifest.
         *
         * @param contents The successfully downloaded payment method manifest.
         */
        @CalledByNative("ManifestDownloadCallback")
        void onManifestDownloadSuccess(String contents);

        /** Called on failed download of a payment method manifest. */
        @CalledByNative("ManifestDownloadCallback")
        void onManifestDownloadFailure();
    }

    private final WebContents mWebContents;

    /**
     * Builds the downloader.
     *
     * @param webContents The web contents to use as the context for the download. If this goes
     *                    away, the download is cancelled.
     */
    public PaymentManifestDownloader(WebContents webContents) {
        mWebContents = webContents;
    }

    /**
     * Downloads the manifest file asynchronously.
     *
     * @param methodName The payment method name that is a URI with HTTPS scheme.
     * @param callback   The callback to invoke when finished downloading.
     */
    public void download(URI methodName, ManifestDownloadCallback callback) {
        nativeDownloadPaymentManifest(mWebContents, methodName, callback);
    }

    @CalledByNative
    private static String getUriString(URI methodName) {
        return methodName.toString();
    }

    private static native void nativeDownloadPaymentManifest(
            WebContents webContents, URI methodName, ManifestDownloadCallback callback);
}
