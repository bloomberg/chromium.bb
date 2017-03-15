// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.Signature;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.payments.PaymentManifestVerifier.ManifestVerifyCallback;
import org.chromium.components.payments.PaymentManifestDownloader;
import org.chromium.components.payments.PaymentManifestParser;
import org.chromium.payments.mojom.PaymentManifestSection;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.List;

/** A test for the verifier of a payment app manifest. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = 21, manifest = Config.NONE)
public class PaymentManifestVerifierTest {
    private final URI mMethodName;
    private final ResolveInfo mAlicePay;
    private final ResolveInfo mBobPay;
    private final List<ResolveInfo> mMatchingApps;
    private final PaymentManifestDownloader mDownloader;
    private final PaymentManifestParser mAnyAppAllowedParser;
    private final PackageManagerDelegate mPackageManagerDelegate;
    private final ManifestVerifyCallback mCallback;

    public PaymentManifestVerifierTest() throws URISyntaxException {
        mMethodName = new URI("https://example.com");

        mAlicePay = new ResolveInfo();
        mAlicePay.activityInfo = new ActivityInfo();
        mAlicePay.activityInfo.packageName = "com.alicepay.app";

        mBobPay = new ResolveInfo();
        mBobPay.activityInfo = new ActivityInfo();
        mBobPay.activityInfo.packageName = "com.bobpay.app";

        mMatchingApps = new ArrayList<>();
        mMatchingApps.add(mAlicePay);
        mMatchingApps.add(mBobPay);

        mDownloader = new PaymentManifestDownloader(null) {
            @Override
            public void download(URI uri, ManifestDownloadCallback callback) {
                callback.onManifestDownloadSuccess("some content here");
            }
        };

        mAnyAppAllowedParser = new PaymentManifestParser() {
            @Override
            public void parse(String content, ManifestParseCallback callback) {
                PaymentManifestSection[] manifest = new PaymentManifestSection[1];
                manifest[0] = new PaymentManifestSection();
                manifest[0].packageName = "*";
                callback.onManifestParseSuccess(manifest);
            }
        };

        mPackageManagerDelegate = Mockito.mock(PackageManagerDelegate.class);

        PackageInfo bobPayPackageInfo = new PackageInfo();
        bobPayPackageInfo.versionCode = 10;
        bobPayPackageInfo.signatures = new Signature[1];
        bobPayPackageInfo.signatures[0] = new Signature("01020304050607080900");
        Mockito.when(mPackageManagerDelegate.getPackageInfoWithSignatures("com.bobpay.app"))
                .thenReturn(bobPayPackageInfo);

        PackageInfo alicePayPackageInfo = new PackageInfo();
        alicePayPackageInfo.versionCode = 10;
        alicePayPackageInfo.signatures = new Signature[1];
        alicePayPackageInfo.signatures[0] = new Signature("ABCDEFABCDEFABCDEFAB");
        Mockito.when(mPackageManagerDelegate.getPackageInfoWithSignatures("com.alicepay.app"))
                .thenReturn(alicePayPackageInfo);

        mCallback = Mockito.mock(ManifestVerifyCallback.class);
    }

    @Test
    public void testUnableToDownload() {
        PaymentManifestVerifier verifier = new PaymentManifestVerifier(
                mMethodName, mMatchingApps, new PaymentManifestDownloader(null) {
                    @Override
                    public void download(URI uri, ManifestDownloadCallback callback) {
                        callback.onManifestDownloadFailure();
                    }
                }, mAnyAppAllowedParser, mPackageManagerDelegate, mCallback);

        verifier.verify();

        Mockito.verify(mCallback).onInvalidManifest(mMethodName);
    }

    @Test
    public void testUnableToParse() {
        PaymentManifestVerifier verifier = new PaymentManifestVerifier(
                mMethodName, mMatchingApps, mDownloader, new PaymentManifestParser() {
                    @Override
                    public void parse(String content, ManifestParseCallback callback) {
                        callback.onManifestParseFailure();
                    }
                }, mPackageManagerDelegate, mCallback);

        verifier.verify();

        Mockito.verify(mCallback).onInvalidManifest(mMethodName);
    }

    @Test
    public void testAnyAppAllowed() {
        PaymentManifestVerifier verifier = new PaymentManifestVerifier(mMethodName, mMatchingApps,
                mDownloader, mAnyAppAllowedParser, mPackageManagerDelegate, mCallback);

        verifier.verify();

        Mockito.verify(mCallback).onValidPaymentApp(mMethodName, mAlicePay);
        Mockito.verify(mCallback).onValidPaymentApp(mMethodName, mBobPay);
    }

    @Test
    public void testBobPayAllowed() {
        PaymentManifestVerifier verifier = new PaymentManifestVerifier(
                mMethodName, mMatchingApps, mDownloader, new PaymentManifestParser() {
                    @Override
                    public void parse(String content, ManifestParseCallback callback) {
                        PaymentManifestSection[] manifest = new PaymentManifestSection[1];
                        manifest[0] = new PaymentManifestSection();
                        manifest[0].packageName = "com.bobpay.app";
                        manifest[0].version = 10;
                        // SHA256("01020304050607080900"):
                        manifest[0].sha256CertFingerprints = new byte[][] {{(byte) 0x9A,
                                (byte) 0x89, (byte) 0xC6, (byte) 0x8C, (byte) 0x4C, (byte) 0x5E,
                                (byte) 0x28, (byte) 0xB8, (byte) 0xC4, (byte) 0xA5, (byte) 0x56,
                                (byte) 0x76, (byte) 0x73, (byte) 0xD4, (byte) 0x62, (byte) 0xFF,
                                (byte) 0xF5, (byte) 0x15, (byte) 0xDB, (byte) 0x46, (byte) 0x11,
                                (byte) 0x6F, (byte) 0x99, (byte) 0x00, (byte) 0x62, (byte) 0x4D,
                                (byte) 0x09, (byte) 0xC4, (byte) 0x74, (byte) 0xF5, (byte) 0x93,
                                (byte) 0xFB}};
                        callback.onManifestParseSuccess(manifest);
                    }
                }, mPackageManagerDelegate, mCallback);

        verifier.verify();

        Mockito.verify(mCallback).onInvalidPaymentApp(mMethodName, mAlicePay);
        Mockito.verify(mCallback).onValidPaymentApp(mMethodName, mBobPay);
    }
}
