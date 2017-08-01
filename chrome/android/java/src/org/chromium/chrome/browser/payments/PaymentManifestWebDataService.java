// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.payments.mojom.WebAppManifestSection;

/** Java wrapper of the payment manifest web data service. */
@JNINamespace("payments")
public class PaymentManifestWebDataService {
    /** Interface for the callback to invoke when getting data from the web data service. */
    public interface PaymentManifestWebDataServiceCallback {
        /**
         * Called when getPaymentMethodManifest success.
         *
         * @param appIdentifiers The list of package names and origins of the supported apps in the
         *                       payment method manifest. May also contain "*" to indicate that
         *                       all origins are supported.
         */
        @CalledByNative("PaymentManifestWebDataServiceCallback")
        void onPaymentMethodManifestFetched(String[] appIdentifiers);

        /**
         * Called when getPaymentWebAppManifest success.
         *
         * @param manifest The web app manifest sections.
         */
        @CalledByNative("PaymentManifestWebDataServiceCallback")
        void onPaymentWebAppManifestFetched(WebAppManifestSection[] manifest);
    }

    /** Holds the native counterpart of this class object. */
    private long mManifestWebDataServiceAndroid;

    public PaymentManifestWebDataService() {
        mManifestWebDataServiceAndroid = nativeInit();
    }

    /**
     * Destroy this class object. It destroys the native counterpart.
     */
    public void destroy() {
        nativeDestroy(mManifestWebDataServiceAndroid);
        mManifestWebDataServiceAndroid = 0;
    }

    /**
     * Gets the payment method's manifest.
     *
     * @param methodName The payment method name.
     * @param callback   The callback to invoke when finishing the request.
     * @return True if the result will be returned through callback.
     */
    public boolean getPaymentMethodManifest(
            String methodName, PaymentManifestWebDataServiceCallback callback) {
        return nativeGetPaymentMethodManifest(mManifestWebDataServiceAndroid, methodName, callback);
    }

    /**
     * Gets the corresponding payment web app's manifest.
     *
     * @param appPackageName The package name of the Android payment app.
     * @param callback       The callback to invoke when finishing the request.
     * @return True if the result will be returned through callback.
     */
    public boolean getPaymentWebAppManifest(
            String appPackageName, PaymentManifestWebDataServiceCallback callback) {
        return nativeGetPaymentWebAppManifest(
                mManifestWebDataServiceAndroid, appPackageName, callback);
    }

    /**
     * Adds the supported Android apps' package names of the method.
     *
     * @param methodName       The method name.
     * @param appPackageNames  The supported app package names and origins. Also possibly "*" if
     *                         applicable.
     */
    public void addPaymentMethodManifest(String methodName, String[] appIdentifiers) {
        nativeAddPaymentMethodManifest(mManifestWebDataServiceAndroid, methodName, appIdentifiers);
    }

    /**
     * Adds web app's manifest.
     *
     * @param manifest The manifest.
     */
    public void addPaymentWebAppManifest(WebAppManifestSection[] manifest) {
        nativeAddPaymentWebAppManifest(mManifestWebDataServiceAndroid, manifest);
    }

    @CalledByNative
    private static WebAppManifestSection[] createManifest(int numberOfsections) {
        return new WebAppManifestSection[numberOfsections];
    }

    @CalledByNative
    private static void addSectionToManifest(WebAppManifestSection[] manifest, int sectionIndex,
            String id, long minVersion, int numberOfFingerprints) {
        manifest[sectionIndex] = new WebAppManifestSection();
        manifest[sectionIndex].id = id;
        manifest[sectionIndex].minVersion = minVersion;
        manifest[sectionIndex].fingerprints = new byte[numberOfFingerprints][];
    }

    @CalledByNative
    private static void addFingerprintToSection(WebAppManifestSection[] manifest, int sectionIndex,
            int fingerprintIndex, byte[] fingerprint) {
        manifest[sectionIndex].fingerprints[fingerprintIndex] = fingerprint;
    }

    @CalledByNative
    private static String getIdFromSection(WebAppManifestSection manifestSection) {
        return manifestSection.id;
    }

    @CalledByNative
    private static long getMinVersionFromSection(WebAppManifestSection manifestSection) {
        return manifestSection.minVersion;
    }

    @CalledByNative
    private static byte[][] getFingerprintsFromSection(WebAppManifestSection manifestSection) {
        return manifestSection.fingerprints;
    }

    private native long nativeInit();
    private native void nativeDestroy(long nativePaymentManifestWebDataServiceAndroid);
    private native boolean nativeGetPaymentMethodManifest(
            long nativePaymentManifestWebDataServiceAndroid, String methodName,
            PaymentManifestWebDataServiceCallback callback);
    private native boolean nativeGetPaymentWebAppManifest(
            long nativePaymentManifestWebDataServiceAndroid, String appPackageName,
            PaymentManifestWebDataServiceCallback callback);
    private native void nativeAddPaymentMethodManifest(
            long nativePaymentManifestWebDataServiceAndroid, String methodName,
            String[] appPackageNames);
    private native void nativeAddPaymentWebAppManifest(
            long nativePaymentManifestWebDataServiceAndroid, WebAppManifestSection[] manifest);
}
