// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.payments.mojom.PaymentManifestSection;

/** Parses payment manifests in a utility process. */
@JNINamespace("payments")
public class PaymentManifestParser {
    /** Interface for the callback to invoke when finished parsing. */
    public interface ManifestParseCallback {
        /**
         * Called on successful parse of a payment method manifest.
         *
         * @param manifest The successfully parsed payment method manifest.
         */
        @CalledByNative("ManifestParseCallback")
        void onManifestParseSuccess(PaymentManifestSection[] manifest);

        /** Called on failed parse of a payment method manifest. */
        @CalledByNative("ManifestParseCallback")
        void onManifestParseFailure();
    }

    /** Owned native host of the utility process that parses manifest contents. */
    private long mNativePaymentManifestParserAndroid;

    /** Starts the utility process. */
    public void startUtilityProcess() {
        assert mNativePaymentManifestParserAndroid == 0;
        mNativePaymentManifestParserAndroid = nativeCreatePaymentManifestParserAndroid();
        nativeStartUtilityProcess(mNativePaymentManifestParserAndroid);
    }

    /** Stops the utility process. */
    public void stopUtilityProcess() {
        assert mNativePaymentManifestParserAndroid != 0;
        nativeStopUtilityProcess(mNativePaymentManifestParserAndroid);
        mNativePaymentManifestParserAndroid = 0;
    }

    /** @return Whether the utility process is running. */
    public boolean isUtilityProcessRunning() {
        return mNativePaymentManifestParserAndroid != 0;
    }

    /**
     * Parses the manifest file asynchronously.
     *
     * @param content  The content to parse.
     * @param callback The callback to invoke when finished parsing.
     */
    public void parse(String content, ManifestParseCallback callback) {
        nativeParsePaymentManifest(mNativePaymentManifestParserAndroid, content, callback);
    }

    @CalledByNative
    private static PaymentManifestSection[] createManifest(int numberOfsections) {
        return new PaymentManifestSection[numberOfsections];
    }

    @CalledByNative
    private static void addSectionToManifest(PaymentManifestSection[] manifest, int sectionIndex,
            String packageName, long version, int numberOfFingerprints) {
        manifest[sectionIndex] = new PaymentManifestSection();
        manifest[sectionIndex].packageName = packageName;
        manifest[sectionIndex].version = version;
        manifest[sectionIndex].sha256CertFingerprints = new byte[numberOfFingerprints][];
    }

    @CalledByNative
    private static void addFingerprintToSection(PaymentManifestSection[] manifest, int sectionIndex,
            int fingerprintIndex, byte[] fingerprint) {
        manifest[sectionIndex].sha256CertFingerprints[fingerprintIndex] = fingerprint;
    }

    private static native long nativeCreatePaymentManifestParserAndroid();
    private static native void nativeStartUtilityProcess(long nativePaymentManifestParserAndroid);
    private static native void nativeParsePaymentManifest(long nativePaymentManifestParserAndroid,
            String content, ManifestParseCallback callback);
    private static native void nativeStopUtilityProcess(long nativePaymentManifestParserAndroid);
}
