// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.pm.Signature;
import android.net.Uri;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.ArgumentMatchers;
import org.mockito.Mockito;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.payments.PaymentAppFactory.PaymentAppCreatedCallback;
import org.chromium.components.payments.PaymentManifestDownloader;
import org.chromium.components.payments.PaymentManifestParser;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentManifestSection;

import java.net.URI;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Tests for the native Android payment app finder. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = 21, manifest = Config.NONE)
public class AndroidPaymentAppFinderTest {
    public AndroidPaymentAppFinderTest() {}

    /**
     * Argument matcher that matches Intents using |filterEquals| method.
     */
    private static class IntentArgumentMatcher implements ArgumentMatcher<Intent> {

        private final Intent mIntent;

        public IntentArgumentMatcher(Intent intent) {
            mIntent = intent;
        }

        @Override
        public boolean matches(Intent other) {
            return mIntent.filterEquals(other);
        }

        @Override
        public String toString() {
            return mIntent.toString();
        }
    }

    @Test
    public void testNoValidPaymentMethodNames() {
        Set<String> methodNames = new HashSet<>();
        methodNames.add("unknown-payment-method-name");
        methodNames.add("http://not.secure.payment.method.name.com");
        methodNames.add("https://"); // Invalid URI.
        PaymentAppCreatedCallback callback = Mockito.mock(PaymentAppCreatedCallback.class);

        AndroidPaymentAppFinder.find(Mockito.mock(WebContents.class), methodNames, false,
                Mockito.mock(PaymentManifestDownloader.class),
                Mockito.mock(PaymentManifestParser.class),
                Mockito.mock(PackageManagerDelegate.class), callback);

        Mockito.verify(callback, Mockito.never())
                .onPaymentAppCreated(Mockito.any(PaymentApp.class));
        Mockito.verify(callback).onAllPaymentAppsCreated();
    }

    @Test
    public void testQueryBasicCardsWithoutApps() {
        PackageManagerDelegate packageManagerDelegate = Mockito.mock(PackageManagerDelegate.class);
        Mockito.when(packageManagerDelegate.getActivitiesThatCanRespondToIntent(
                ArgumentMatchers.argThat(new IntentArgumentMatcher(
                        new Intent(AndroidPaymentAppFinder.ACTION_PAY_BASIC_CARD)))))
                .thenReturn(new ArrayList<ResolveInfo>());
        Set<String> methodNames = new HashSet<>();
        methodNames.add(AndroidPaymentAppFinder.BASIC_CARD_PAYMENT_METHOD);
        PaymentAppCreatedCallback callback = Mockito.mock(PaymentAppCreatedCallback.class);

        AndroidPaymentAppFinder.find(Mockito.mock(WebContents.class), methodNames, false,
                Mockito.mock(PaymentManifestDownloader.class),
                Mockito.mock(PaymentManifestParser.class), packageManagerDelegate, callback);

        Mockito.verify(callback, Mockito.never())
                .onPaymentAppCreated(Mockito.any(PaymentApp.class));
        Mockito.verify(callback).onAllPaymentAppsCreated();
    }

    @Test
    public void testQueryBasicCardsWithTwoApps() {
        List<ResolveInfo> activities = new ArrayList<>();
        ResolveInfo alicePay = new ResolveInfo();
        alicePay.activityInfo = new ActivityInfo();
        alicePay.activityInfo.packageName = "com.alicepay.app";
        alicePay.activityInfo.name = "com.alicepay.app.WebPaymentActivity";
        activities.add(alicePay);
        ResolveInfo bobPay = new ResolveInfo();
        bobPay.activityInfo = new ActivityInfo();
        bobPay.activityInfo.packageName = "com.bobpay.app";
        bobPay.activityInfo.name = "com.bobpay.app.WebPaymentActivity";
        activities.add(bobPay);
        PackageManagerDelegate packageManagerDelegate = Mockito.mock(PackageManagerDelegate.class);
        Mockito.when(packageManagerDelegate.getAppLabel(Mockito.any(ResolveInfo.class)))
                .thenReturn("A non-empty label");
        Mockito.when(packageManagerDelegate.getActivitiesThatCanRespondToIntent(
                ArgumentMatchers.argThat(new IntentArgumentMatcher(
                        new Intent(AndroidPaymentAppFinder.ACTION_PAY_BASIC_CARD)))))
                .thenReturn(activities);
        Mockito.when(packageManagerDelegate.getServicesThatCanRespondToIntent(
                ArgumentMatchers.argThat(new IntentArgumentMatcher(
                        new Intent(AndroidPaymentAppFinder.ACTION_IS_READY_TO_PAY)))))
                .thenReturn(new ArrayList<ResolveInfo>());
        Set<String> methodNames = new HashSet<>();
        methodNames.add(AndroidPaymentAppFinder.BASIC_CARD_PAYMENT_METHOD);
        PaymentAppCreatedCallback callback = Mockito.mock(PaymentAppCreatedCallback.class);

        AndroidPaymentAppFinder.find(Mockito.mock(WebContents.class), methodNames, false,
                Mockito.mock(PaymentManifestDownloader.class),
                Mockito.mock(PaymentManifestParser.class), packageManagerDelegate, callback);

        Mockito.verify(callback).onPaymentAppCreated(
                ArgumentMatchers.argThat(Matches.paymentAppIdentifier("com.alicepay.app")));
        Mockito.verify(callback).onPaymentAppCreated(
                ArgumentMatchers.argThat(Matches.paymentAppIdentifier("com.bobpay.app")));
        Mockito.verify(callback).onAllPaymentAppsCreated();
    }

    @Test
    public void testQueryBobPayWithoutApps() {
        Intent bobPayIntent = new Intent("org.chromium.intent.action.PAY");
        bobPayIntent.setData(Uri.parse("https://bobpay.com"));
        PackageManagerDelegate packageManagerDelegate = Mockito.mock(PackageManagerDelegate.class);
        Mockito.when(packageManagerDelegate.getActivitiesThatCanRespondToIntent(bobPayIntent))
                .thenReturn(new ArrayList<ResolveInfo>());
        Set<String> methodNames = new HashSet<>();
        methodNames.add("https://bobpay.com");
        PaymentAppCreatedCallback callback = Mockito.mock(PaymentAppCreatedCallback.class);

        AndroidPaymentAppFinder.find(Mockito.mock(WebContents.class), methodNames, false,
                Mockito.mock(PaymentManifestDownloader.class),
                Mockito.mock(PaymentManifestParser.class), packageManagerDelegate, callback);

        Mockito.verify(callback, Mockito.never())
                .onPaymentAppCreated(Mockito.any(PaymentApp.class));
        Mockito.verify(callback).onAllPaymentAppsCreated();
    }

    @Test
    public void testQueryBobPayWithOneAppThatHasIsReadyToPayService() {
        List<ResolveInfo> activities = new ArrayList<>();
        ResolveInfo bobPay = new ResolveInfo();
        bobPay.activityInfo = new ActivityInfo();
        bobPay.activityInfo.packageName = "com.bobpay.app";
        bobPay.activityInfo.name = "com.bobpay.app.WebPaymentActivity";
        activities.add(bobPay);
        Intent bobPayIntent = new Intent("org.chromium.intent.action.PAY");
        bobPayIntent.setData(Uri.parse("https://bobpay.com"));
        PackageManagerDelegate packageManagerDelegate = Mockito.mock(PackageManagerDelegate.class);
        Mockito.when(packageManagerDelegate.getAppLabel(Mockito.any(ResolveInfo.class)))
                .thenReturn("A non-empty label");
        Mockito.when(packageManagerDelegate.getActivitiesThatCanRespondToIntent(
                ArgumentMatchers.argThat(new IntentArgumentMatcher(bobPayIntent))))
                .thenReturn(activities);

        List<ResolveInfo> services = new ArrayList<>();
        ResolveInfo isBobPayReadyToPay = new ResolveInfo();
        isBobPayReadyToPay.serviceInfo = new ServiceInfo();
        isBobPayReadyToPay.serviceInfo.packageName = "com.bobpay.app";
        isBobPayReadyToPay.serviceInfo.name = "com.bobpay.app.IsReadyToWebPay";
        services.add(isBobPayReadyToPay);
        Intent isReadyToPayIntent = new Intent(AndroidPaymentAppFinder.ACTION_IS_READY_TO_PAY);
        Mockito.when(packageManagerDelegate.getServicesThatCanRespondToIntent(
                ArgumentMatchers.argThat(new IntentArgumentMatcher(isReadyToPayIntent))))
                .thenReturn(services);

        PackageInfo bobPayPackageInfo = new PackageInfo();
        bobPayPackageInfo.versionCode = 10;
        bobPayPackageInfo.signatures = new Signature[1];
        bobPayPackageInfo.signatures[0] = new Signature("01020304050607080900");
        Mockito.when(packageManagerDelegate.getPackageInfoWithSignatures("com.bobpay.app"))
                .thenReturn(bobPayPackageInfo);

        PaymentManifestDownloader downloader = new PaymentManifestDownloader(null) {
            @Override
            public void download(URI uri, ManifestDownloadCallback callback) {
                callback.onManifestDownloadSuccess("some content here");
            }
        };

        PaymentManifestParser parser = new PaymentManifestParser() {
            @Override
            public void parse(String content, ManifestParseCallback callback) {
                PaymentManifestSection[] manifest = new PaymentManifestSection[1];
                manifest[0] = new PaymentManifestSection();
                manifest[0].packageName = "com.bobpay.app";
                manifest[0].version = 10;
                // SHA256("01020304050607080900"):
                manifest[0].sha256CertFingerprints = new byte[][] {{(byte) 0x9A, (byte) 0x89,
                        (byte) 0xC6, (byte) 0x8C, (byte) 0x4C, (byte) 0x5E, (byte) 0x28,
                        (byte) 0xB8, (byte) 0xC4, (byte) 0xA5, (byte) 0x56, (byte) 0x76,
                        (byte) 0x73, (byte) 0xD4, (byte) 0x62, (byte) 0xFF, (byte) 0xF5,
                        (byte) 0x15, (byte) 0xDB, (byte) 0x46, (byte) 0x11, (byte) 0x6F,
                        (byte) 0x99, (byte) 0x00, (byte) 0x62, (byte) 0x4D, (byte) 0x09,
                        (byte) 0xC4, (byte) 0x74, (byte) 0xF5, (byte) 0x93, (byte) 0xFB}};
                callback.onManifestParseSuccess(manifest);
            }

            @Override
            public void startUtilityProcess() {}

            @Override
            public void stopUtilityProcess() {}
        };

        Set<String> methodNames = new HashSet<>();
        methodNames.add("https://bobpay.com");
        PaymentAppCreatedCallback callback = Mockito.mock(PaymentAppCreatedCallback.class);

        AndroidPaymentAppFinder.find(Mockito.mock(WebContents.class), methodNames, false,
                downloader, parser, packageManagerDelegate, callback);

        Mockito.verify(callback).onPaymentAppCreated(
                ArgumentMatchers.argThat(Matches.paymentAppIdentifier("com.bobpay.app")));
        Mockito.verify(callback).onAllPaymentAppsCreated();
    }

    private static final class Matches implements ArgumentMatcher<PaymentApp> {
        private final String mExpectedAppIdentifier;

        private Matches(String expectedAppIdentifier) {
            mExpectedAppIdentifier = expectedAppIdentifier;
        }

        /**
         * Builds a matcher based on payment app identifier.
         *
         * @param expectedAppIdentifier The expected app identifier to match.
         * @return A matcher to use in a mock expectation.
         */
        public static ArgumentMatcher<PaymentApp> paymentAppIdentifier(
                String expectedAppIdentifier) {
            return new Matches(expectedAppIdentifier);
        }

        @Override
        public boolean matches(PaymentApp app) {
            return app.getAppIdentifier().equals(mExpectedAppIdentifier);
        }
    }
}
