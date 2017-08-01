// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.pm.Signature;
import android.os.Bundle;

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
import org.chromium.payments.mojom.WebAppManifestSection;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Tests for the native Android payment app finder. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = 21, manifest = Config.NONE)
public class AndroidPaymentAppFinderTest {
    private static final IntentArgumentMatcher sPayIntentArgumentMatcher =
            new IntentArgumentMatcher(new Intent("org.chromium.intent.action.PAY"));

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

        AndroidPaymentAppFinder.find(Mockito.mock(WebContents.class), methodNames,
                Mockito.mock(PaymentManifestWebDataService.class),
                Mockito.mock(PaymentManifestDownloader.class),
                Mockito.mock(PaymentManifestParser.class),
                Mockito.mock(PackageManagerDelegate.class), callback);

        Mockito.verify(callback, Mockito.never())
                .onPaymentAppCreated(Mockito.any(PaymentApp.class));
        Mockito.verify(callback).onAllPaymentAppsCreated();
    }

    @Test
    public void testQueryWithoutApps() {
        PackageManagerDelegate packageManagerDelegate = Mockito.mock(PackageManagerDelegate.class);
        Mockito.when(packageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                             ArgumentMatchers.argThat(sPayIntentArgumentMatcher)))
                .thenReturn(new ArrayList<ResolveInfo>());
        Set<String> methodNames = new HashSet<>();
        methodNames.add("basic-card");
        PaymentAppCreatedCallback callback = Mockito.mock(PaymentAppCreatedCallback.class);

        AndroidPaymentAppFinder.find(Mockito.mock(WebContents.class), methodNames,
                Mockito.mock(PaymentManifestWebDataService.class),
                Mockito.mock(PaymentManifestDownloader.class),
                Mockito.mock(PaymentManifestParser.class), packageManagerDelegate, callback);

        Mockito.verify(packageManagerDelegate, Mockito.never())
                .getStringArrayResourceForApplication(
                        ArgumentMatchers.any(ApplicationInfo.class), ArgumentMatchers.anyInt());
        Mockito.verify(callback, Mockito.never())
                .onPaymentAppCreated(Mockito.any(PaymentApp.class));
        Mockito.verify(callback).onAllPaymentAppsCreated();
    }

    @Test
    public void testQueryWithoutMetaData() {
        List<ResolveInfo> activities = new ArrayList<>();
        ResolveInfo alicePay = new ResolveInfo();
        alicePay.activityInfo = new ActivityInfo();
        alicePay.activityInfo.packageName = "com.alicepay.app";
        alicePay.activityInfo.name = "com.alicepay.app.WebPaymentActivity";
        activities.add(alicePay);

        PackageManagerDelegate packageManagerDelegate = Mockito.mock(PackageManagerDelegate.class);
        Mockito.when(packageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                             ArgumentMatchers.argThat(sPayIntentArgumentMatcher)))
                .thenReturn(activities);

        Set<String> methodNames = new HashSet<>();
        methodNames.add("basic-card");
        PaymentAppCreatedCallback callback = Mockito.mock(PaymentAppCreatedCallback.class);

        AndroidPaymentAppFinder.find(Mockito.mock(WebContents.class), methodNames,
                Mockito.mock(PaymentManifestWebDataService.class),
                Mockito.mock(PaymentManifestDownloader.class),
                Mockito.mock(PaymentManifestParser.class), packageManagerDelegate, callback);

        Mockito.verify(packageManagerDelegate, Mockito.never())
                .getStringArrayResourceForApplication(
                        ArgumentMatchers.any(ApplicationInfo.class), ArgumentMatchers.anyInt());
        Mockito.verify(callback, Mockito.never())
                .onPaymentAppCreated(Mockito.any(PaymentApp.class));
        Mockito.verify(callback).onAllPaymentAppsCreated();
    }

    @Test
    public void testQueryWithUnsupportedPaymentMethod() {
        List<ResolveInfo> activities = new ArrayList<>();
        ResolveInfo alicePay = new ResolveInfo();
        alicePay.activityInfo = new ActivityInfo();
        alicePay.activityInfo.packageName = "com.alicepay.app";
        alicePay.activityInfo.name = "com.alicepay.app.WebPaymentActivity";
        Bundle activityMetaData = new Bundle();
        activityMetaData.putString(
                AndroidPaymentAppFinder.META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME,
                "basic-card");
        alicePay.activityInfo.metaData = activityMetaData;
        activities.add(alicePay);

        PackageManagerDelegate packageManagerDelegate = Mockito.mock(PackageManagerDelegate.class);
        Mockito.when(packageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                             ArgumentMatchers.argThat(sPayIntentArgumentMatcher)))
                .thenReturn(activities);

        Set<String> methodNames = new HashSet<>();
        methodNames.add("basic-card");
        PaymentAppCreatedCallback callback = Mockito.mock(PaymentAppCreatedCallback.class);

        AndroidPaymentAppFinder.find(Mockito.mock(WebContents.class), methodNames,
                Mockito.mock(PaymentManifestWebDataService.class),
                Mockito.mock(PaymentManifestDownloader.class),
                Mockito.mock(PaymentManifestParser.class), packageManagerDelegate, callback);

        Mockito.verify(packageManagerDelegate, Mockito.never())
                .getStringArrayResourceForApplication(
                        ArgumentMatchers.any(ApplicationInfo.class), ArgumentMatchers.anyInt());
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
        alicePay.activityInfo.applicationInfo = new ApplicationInfo();
        Bundle alicePayMetaData = new Bundle();
        alicePayMetaData.putString(
                AndroidPaymentAppFinder.META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME,
                "basic-card");
        alicePayMetaData.putInt(AndroidPaymentAppFinder.META_DATA_NAME_OF_PAYMENT_METHOD_NAMES, 1);
        alicePay.activityInfo.metaData = alicePayMetaData;
        activities.add(alicePay);

        ResolveInfo bobPay = new ResolveInfo();
        bobPay.activityInfo = new ActivityInfo();
        bobPay.activityInfo.packageName = "com.bobpay.app";
        bobPay.activityInfo.name = "com.bobpay.app.WebPaymentActivity";
        bobPay.activityInfo.applicationInfo = new ApplicationInfo();
        Bundle bobPayMetaData = new Bundle();
        bobPayMetaData.putString(
                AndroidPaymentAppFinder.META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME,
                "basic-card");
        bobPayMetaData.putInt(AndroidPaymentAppFinder.META_DATA_NAME_OF_PAYMENT_METHOD_NAMES, 2);
        bobPay.activityInfo.metaData = bobPayMetaData;
        activities.add(bobPay);

        PackageManagerDelegate packageManagerDelegate = Mockito.mock(PackageManagerDelegate.class);
        Mockito.when(packageManagerDelegate.getAppLabel(Mockito.any(ResolveInfo.class)))
                .thenReturn("A non-empty label");
        Mockito.when(packageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                             ArgumentMatchers.argThat(sPayIntentArgumentMatcher)))
                .thenReturn(activities);
        Mockito.when(packageManagerDelegate.getServicesThatCanRespondToIntent(
                             ArgumentMatchers.argThat(new IntentArgumentMatcher(
                                     new Intent(AndroidPaymentAppFinder.ACTION_IS_READY_TO_PAY)))))
                .thenReturn(new ArrayList<ResolveInfo>());

        Mockito.when(packageManagerDelegate.getStringArrayResourceForApplication(
                             ArgumentMatchers.eq(alicePay.activityInfo.applicationInfo),
                             ArgumentMatchers.eq(1)))
                .thenReturn(new String[] {"https://alicepay.com"});
        Mockito.when(packageManagerDelegate.getStringArrayResourceForApplication(
                             ArgumentMatchers.eq(bobPay.activityInfo.applicationInfo),
                             ArgumentMatchers.eq(2)))
                .thenReturn(new String[] {"https://bobpay.com"});

        Set<String> methodNames = new HashSet<>();
        methodNames.add("basic-card");
        PaymentAppCreatedCallback callback = Mockito.mock(PaymentAppCreatedCallback.class);

        AndroidPaymentAppFinder.find(Mockito.mock(WebContents.class), methodNames,
                Mockito.mock(PaymentManifestWebDataService.class),
                Mockito.mock(PaymentManifestDownloader.class),
                Mockito.mock(PaymentManifestParser.class), packageManagerDelegate, callback);

        Mockito.verify(callback).onPaymentAppCreated(
                ArgumentMatchers.argThat(Matches.paymentAppIdentifier("com.alicepay.app")));
        Mockito.verify(callback).onPaymentAppCreated(
                ArgumentMatchers.argThat(Matches.paymentAppIdentifier("com.bobpay.app")));
        Mockito.verify(callback).onAllPaymentAppsCreated();
    }

    @Test
    public void testQueryBobPayWithOneAppThatHasIsReadyToPayService() {
        List<ResolveInfo> activities = new ArrayList<>();
        ResolveInfo bobPay = new ResolveInfo();
        bobPay.activityInfo = new ActivityInfo();
        bobPay.activityInfo.packageName = "com.bobpay.app";
        bobPay.activityInfo.name = "com.bobpay.app.WebPaymentActivity";
        bobPay.activityInfo.applicationInfo = new ApplicationInfo();
        Bundle bobPayMetaData = new Bundle();
        bobPayMetaData.putString(
                AndroidPaymentAppFinder.META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME,
                "https://bobpay.com");
        bobPayMetaData.putInt(AndroidPaymentAppFinder.META_DATA_NAME_OF_PAYMENT_METHOD_NAMES, 1);
        bobPay.activityInfo.metaData = bobPayMetaData;
        activities.add(bobPay);

        PackageManagerDelegate packageManagerDelegate = Mockito.mock(PackageManagerDelegate.class);
        Mockito.when(packageManagerDelegate.getAppLabel(Mockito.any(ResolveInfo.class)))
                .thenReturn("A non-empty label");
        Mockito.when(packageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                             ArgumentMatchers.argThat(sPayIntentArgumentMatcher)))
                .thenReturn(activities);

        Mockito.when(packageManagerDelegate.getStringArrayResourceForApplication(
                             ArgumentMatchers.eq(bobPay.activityInfo.applicationInfo),
                             ArgumentMatchers.eq(1)))
                .thenReturn(new String[] {"https://bobpay.com", "basic-card"});

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

        PaymentManifestDownloader downloader = new PaymentManifestDownloader() {
            @Override
            public void initialize(WebContents webContents) {}

            @Override
            public void downloadPaymentMethodManifest(URI uri, ManifestDownloadCallback callback) {
                callback.onPaymentMethodManifestDownloadSuccess("some content here");
            }

            @Override
            public void downloadWebAppManifest(URI uri, ManifestDownloadCallback callback) {
                callback.onWebAppManifestDownloadSuccess("some content here");
            }

            @Override
            public void destroy() {}
        };

        PaymentManifestParser parser = new PaymentManifestParser() {
            @Override
            public void parsePaymentMethodManifest(String content, ManifestParseCallback callback) {
                try {
                    callback.onPaymentMethodManifestParseSuccess(
                            new URI[] {new URI("https://bobpay.com/app.json")}, new URI[0], false);
                } catch (URISyntaxException e) {
                    assert false;
                }
            }

            @Override
            public void parseWebAppManifest(String content, ManifestParseCallback callback) {
                WebAppManifestSection[] manifest = new WebAppManifestSection[1];
                manifest[0] = new WebAppManifestSection();
                manifest[0].id = "com.bobpay.app";
                manifest[0].minVersion = 10;
                // SHA256("01020304050607080900"):
                manifest[0].fingerprints = new byte[][] {{(byte) 0x9A, (byte) 0x89, (byte) 0xC6,
                        (byte) 0x8C, (byte) 0x4C, (byte) 0x5E, (byte) 0x28, (byte) 0xB8,
                        (byte) 0xC4, (byte) 0xA5, (byte) 0x56, (byte) 0x76, (byte) 0x73,
                        (byte) 0xD4, (byte) 0x62, (byte) 0xFF, (byte) 0xF5, (byte) 0x15,
                        (byte) 0xDB, (byte) 0x46, (byte) 0x11, (byte) 0x6F, (byte) 0x99,
                        (byte) 0x00, (byte) 0x62, (byte) 0x4D, (byte) 0x09, (byte) 0xC4,
                        (byte) 0x74, (byte) 0xF5, (byte) 0x93, (byte) 0xFB}};
                callback.onWebAppManifestParseSuccess(manifest);
            }

            @Override
            public void startUtilityProcess() {}

            @Override
            public void stopUtilityProcess() {}
        };

        Set<String> methodNames = new HashSet<>();
        methodNames.add("https://bobpay.com");
        PaymentAppCreatedCallback callback = Mockito.mock(PaymentAppCreatedCallback.class);

        AndroidPaymentAppFinder.find(Mockito.mock(WebContents.class), methodNames,
                Mockito.mock(PaymentManifestWebDataService.class), downloader, parser,
                packageManagerDelegate, callback);

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
