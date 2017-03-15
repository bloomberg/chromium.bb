// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.pm.PackageInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.Signature;

import org.chromium.base.Log;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.components.payments.PaymentManifestDownloader;
import org.chromium.components.payments.PaymentManifestDownloader.ManifestDownloadCallback;
import org.chromium.components.payments.PaymentManifestParser;
import org.chromium.components.payments.PaymentManifestParser.ManifestParseCallback;
import org.chromium.payments.mojom.PaymentManifestSection;

import java.net.URI;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Formatter;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Verifies that the discovered native Android payment apps have the sufficient privileges
 * to handle a single payment method. Downloads and parses the manifest to compare package
 * names, versions, and signatures to the apps.
 *
 * Spec:
 * https://docs.google.com/document/d/1izV4uC-tiRJG3JLooqY3YRLU22tYOsLTNq0P_InPJeE/edit#heading=h.cjp3jlnl47h5
 */
public class PaymentManifestVerifier implements ManifestDownloadCallback, ManifestParseCallback {
    private static final String TAG = "cr_PaymentManifest";

    /** Interface for the callback to invoke when finished verification. */
    public interface ManifestVerifyCallback {
        /**
         * Enables invoking the given native Android payment app for the given payment method.
         * Called when the app has been found to have the right privileges to handle this payment
         * method.
         *
         * @param methodName  The payment method name that the payment app offers to handle.
         * @param resolveInfo Identifying information for the native Android payment app.
         */
        void onValidPaymentApp(URI methodName, ResolveInfo resolveInfo);

        /**
         * Disables invoking the given native Android payment app for the given payment method.
         * Called when the app has been found to not have the right privileges to handle this
         * payment app.
         *
         * @param methodName  The payment method name that the payment app offers to handle.
         * @param resolveInfo Identifying information for the native Android payment app.
         */
        void onInvalidPaymentApp(URI methodName, ResolveInfo resolveInfo);

        /**
         * Disables invoking any native Android payment app for the given payment method. Called if
         * unable to download or parse the payment method manifest.
         *
         * @param methodName The payment method name that has an invalid payment method manifest.
         */
        void onInvalidManifest(URI methodName);
    }

    /** Identifying information about an installed native Android payment app. */
    private static class AppInfo {
        /** Identifies a native Android payment app. */
        public ResolveInfo resolveInfo;

        /** The version code for the native Android payment app, e.g., 123. */
        public long version;

        /**
         * The SHA256 certificate fingerprints for the native Android payment app, .e.g,
         * ["308201dd30820146020101300d06092a864886f70d010105050030"].
         */
        public Set<String> sha256CertFingerprints;
    }

    private final PaymentManifestDownloader mDownloader;
    private final URI mMethodName;
    private final List<AppInfo> mMatchingApps;
    private final PaymentManifestParser mParser;
    private final PackageManagerDelegate mPackageManagerDelegate;
    private final ManifestVerifyCallback mCallback;
    private final MessageDigest mMessageDigest;

    /**
     * Builds the manifest verifier.
     *
     * @param methodName             The name of the payment method name that apps offer to handle.
     *                               Must be an absolute URI with HTTPS scheme.
     * @param matchingApps           The identifying information for the native Android payment apps
     *                               that offer to handle this payment method.
     * @param downloader             The manifest downloader.
     * @param parser                 The manifest parser.
     * @param packageManagerDelegate The package information retriever.
     * @param callback               The callback to be notified of verification result.
     */
    public PaymentManifestVerifier(URI methodName, List<ResolveInfo> matchingApps,
            PaymentManifestDownloader downloader, PaymentManifestParser parser,
            PackageManagerDelegate packageManagerDelegate, ManifestVerifyCallback callback) {
        assert methodName.isAbsolute();
        assert UrlConstants.HTTPS_SCHEME.equals(methodName.getScheme());
        assert !matchingApps.isEmpty();

        mMethodName = methodName;
        mMatchingApps = new ArrayList<>();
        for (int i = 0; i < matchingApps.size(); i++) {
            AppInfo appInfo = new AppInfo();
            appInfo.resolveInfo = matchingApps.get(i);
            mMatchingApps.add(appInfo);
        }
        mDownloader = downloader;
        mParser = parser;
        mPackageManagerDelegate = packageManagerDelegate;
        mCallback = callback;

        MessageDigest md = null;
        try {
            md = MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException e) {
            // Intentionally ignore.
            Log.d(TAG, "Unable to generate SHA-256 hashes. Only \"package\": \"*\" supported.");
        }
        mMessageDigest = md;
    }

    /**
     * Verifies that the discovered native Android payment apps have the sufficient
     * privileges to handle this payment method.
     */
    public void verify() {
        mDownloader.download(mMethodName, this);
    }

    @Override
    public void onManifestDownloadSuccess(String content) {
        mParser.parse(content, this);
    }

    @Override
    public void onManifestDownloadFailure() {
        mCallback.onInvalidManifest(mMethodName);
    }

    @Override
    public void onManifestParseSuccess(PaymentManifestSection[] manifest) {
        assert manifest != null;
        assert manifest.length > 0;

        for (int i = 0; i < manifest.length; i++) {
            PaymentManifestSection section = manifest[i];
            // "package": "*" in the manifest file indicates an unrestricted payment method. Any app
            // can use this payment method name.
            if ("*".equals(section.packageName)) {
                for (int j = 0; j < mMatchingApps.size(); j++) {
                    mCallback.onValidPaymentApp(mMethodName, mMatchingApps.get(j).resolveInfo);
                }
                return;
            }
        }

        if (mMessageDigest == null) {
            mCallback.onInvalidManifest(mMethodName);
            return;
        }

        for (int i = 0; i < mMatchingApps.size(); i++) {
            AppInfo appInfo = mMatchingApps.get(i);
            PackageInfo packageInfo = mPackageManagerDelegate.getPackageInfoWithSignatures(
                    appInfo.resolveInfo.activityInfo.packageName);

            // Leaving appInfo.sha256CertFingerprints uninitialized will call onInvalidPaymentApp()
            // for this app below.
            if (packageInfo == null) continue;

            appInfo.version = packageInfo.versionCode;
            appInfo.sha256CertFingerprints = new HashSet<>();
            Signature[] signatures = packageInfo.signatures;
            for (int j = 0; j < signatures.length; j++) {
                mMessageDigest.update(signatures[j].toByteArray());

                // The digest is reset after completing the hash computation.
                appInfo.sha256CertFingerprints.add(byteArrayToString(mMessageDigest.digest()));
            }
        }

        List<Set<String>> sectionsFingerprints = new ArrayList<>();
        for (int i = 0; i < manifest.length; i++) {
            PaymentManifestSection section = manifest[i];
            Set<String> fingerprints = new HashSet<>();
            if (section.sha256CertFingerprints != null) {
                for (int j = 0; j < section.sha256CertFingerprints.length; j++) {
                    fingerprints.add(byteArrayToString(section.sha256CertFingerprints[j]));
                }
            }
            sectionsFingerprints.add(fingerprints);
        }

        for (int i = 0; i < mMatchingApps.size(); i++) {
            AppInfo appInfo = mMatchingApps.get(i);
            boolean isAllowed = false;
            for (int j = 0; j < manifest.length; j++) {
                PaymentManifestSection section = manifest[j];
                if (appInfo.resolveInfo.activityInfo.packageName.equals(section.packageName)
                        && appInfo.version >= section.version
                        && appInfo.sha256CertFingerprints != null
                        && appInfo.sha256CertFingerprints.equals(sectionsFingerprints.get(j))) {
                    mCallback.onValidPaymentApp(mMethodName, appInfo.resolveInfo);
                    isAllowed = true;
                    break;
                }
            }
            if (!isAllowed) mCallback.onInvalidPaymentApp(mMethodName, appInfo.resolveInfo);
        }
    }

    /**
     * Formats bytes into a string for easier comparison as a member of a set.
     *
     * @param input Input bytes.
     * @return A string representation of the input bytes, e.g., "0123456789abcdef".
     */
    private static String byteArrayToString(byte[] input) {
        if (input == null) return null;

        StringBuilder builder = new StringBuilder(input.length * 2);
        Formatter formatter = new Formatter(builder);
        for (byte b : input) {
            formatter.format("%02x", b);
        }

        return builder.toString();
    }

    @Override
    public void onManifestParseFailure() {
        mCallback.onInvalidManifest(mMethodName);
    }
}
