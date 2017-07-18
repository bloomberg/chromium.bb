// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import junit.framework.Assert;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.payments.PaymentManifestDownloader;
import org.chromium.components.payments.PaymentManifestDownloader.ManifestDownloadCallback;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;

import java.net.URI;

/** An integration test for the payment manifest downloader. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
@MediumTest
public class PaymentManifestDownloaderTest implements ManifestDownloadCallback {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String PAYMENT_METHOD_MANIFEST = "{\n"
            + "  \"default_applications\": [\"app.json\"],\n"
            + "  \"supported_origins\": [\"https://alicepay.com\"]\n"
            + "}\n";

    private static final String WEB_APP_MANIFEST = "{\n"
            + "  \"name\": \"BobPay\",\n"
            + "  \"related_applications\": [{\n"
            + "    \"platform\": \"play\",\n"
            + "    \"id\": \"com.bobpay\",\n"
            + "    \"min_version\": \"1\",\n"
            + "    \"fingerprints\": [{\n"
            + "      \"type\": \"sha256_cert\",\n"
            + "      \"value\": \"59:5C:88:65:FF:C4:E8:20:CF:F7:3E:C8:64:D0"
            + ":95:F0:06:19:2E:A6:7B:20:04:D1:03:07:92:E2:A5:31:67:66\"\n"
            + "    }]\n"
            + "  }]\n"
            + "}\n";

    private final PaymentManifestDownloader mDownloader = new PaymentManifestDownloader();
    private EmbeddedTestServer mServer;
    private boolean mDownloadComplete;
    private boolean mDownloadPaymentMethodManifestSuccess;
    private boolean mDownloadWebAppManifestSuccess;
    private boolean mDownloadFailure;
    private String mPaymentMethodManifest;
    private String mWebAppManifest;

    @Override
    public void onPaymentMethodManifestDownloadSuccess(String content) {
        mDownloadComplete = true;
        mDownloadPaymentMethodManifestSuccess = true;
        mPaymentMethodManifest = content;
    }

    @Override
    public void onWebAppManifestDownloadSuccess(String content) {
        mDownloadComplete = true;
        mDownloadWebAppManifestSuccess = true;
        mWebAppManifest = content;
    }

    @Override
    public void onManifestDownloadFailure() {
        mDownloadComplete = true;
        mDownloadFailure = true;
    }

    @Before
    public void setUp() throws Throwable {
        mRule.startMainActivityOnBlankPage();
        mServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        mRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mDownloader.initialize(
                        mRule.getActivity().getCurrentContentViewCore().getWebContents());
                mDownloader.allowHttpForTest();
            }
        });
        mDownloadComplete = false;
        mDownloadPaymentMethodManifestSuccess = false;
        mDownloadWebAppManifestSuccess = false;
        mDownloadFailure = false;
        mPaymentMethodManifest = null;
        mWebAppManifest = null;
    }

    @After
    public void tearDown() throws Throwable {
        mRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mDownloader.destroy();
            }
        });
        mServer.stopAndDestroyServer();
    }

    @Test
    @Feature({"Payments"})
    public void testDownloadWebAppManifest() throws Throwable {
        final URI uri = new URI(mServer.getURL("/components/test/data/payments/app.json"));
        mRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mDownloader.downloadWebAppManifest(uri, PaymentManifestDownloaderTest.this);
            }
        });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mDownloadComplete;
            }
        });

        Assert.assertTrue(
                "Web app manifest should have been downloaded.", mDownloadWebAppManifestSuccess);
        Assert.assertEquals(WEB_APP_MANIFEST, mWebAppManifest);
    }

    @Test
    @Feature({"Payments"})
    public void testUnableToDownloadWebAppManifest() throws Throwable {
        final URI uri = new URI(mServer.getURL("/no-such-app.json"));
        mRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mDownloader.downloadWebAppManifest(uri, PaymentManifestDownloaderTest.this);
            }
        });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mDownloadComplete;
            }
        });

        Assert.assertTrue("Web app manifest should not have been downloaded.", mDownloadFailure);
    }

    @Test
    @Feature({"Payments"})
    public void testDownloadPaymentMethodManifest() throws Throwable {
        final URI uri = new URI(mServer.getURL("/components/test/data/payments/webpay"));
        mRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mDownloader.downloadPaymentMethodManifest(uri, PaymentManifestDownloaderTest.this);
            }
        });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mDownloadComplete;
            }
        });

        Assert.assertTrue("Payment method manifest should have been downloaded.",
                mDownloadPaymentMethodManifestSuccess);
        Assert.assertEquals(PAYMENT_METHOD_MANIFEST, mPaymentMethodManifest);
    }

    @Test
    @Feature({"Payments"})
    public void testUnableToDownloadPaymentMethodManifest() throws Throwable {
        final URI uri = new URI(mServer.getURL("/no-such-payment-method-name"));
        mRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mDownloader.downloadPaymentMethodManifest(uri, PaymentManifestDownloaderTest.this);
            }
        });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mDownloadComplete;
            }
        });

        Assert.assertTrue(
                "Payment method manifest should have not have been downloaded.", mDownloadFailure);
    }

    @Test
    @Feature({"Payments"})
    public void testSeveralDownloadsAtOnce() throws Throwable {
        final URI paymentMethodUri1 = new URI(mServer.getURL("/no-such-payment-method-name"));
        final URI paymentMethodUri2 =
                new URI(mServer.getURL("/components/test/data/payments/webpay"));
        final URI webAppUri1 = new URI(mServer.getURL("/no-such-app.json"));
        final URI webAppUri2 = new URI(mServer.getURL("/components/test/data/payments/app.json"));
        mRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mDownloader.downloadPaymentMethodManifest(
                        paymentMethodUri1, PaymentManifestDownloaderTest.this);
                mDownloader.downloadPaymentMethodManifest(
                        paymentMethodUri2, PaymentManifestDownloaderTest.this);
                mDownloader.downloadWebAppManifest(webAppUri1, PaymentManifestDownloaderTest.this);
                mDownloader.downloadWebAppManifest(webAppUri2, PaymentManifestDownloaderTest.this);
            }
        });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mDownloadWebAppManifestSuccess && mDownloadPaymentMethodManifestSuccess
                        && mDownloadFailure;
            }
        });

        Assert.assertEquals(PAYMENT_METHOD_MANIFEST, mPaymentMethodManifest);
        Assert.assertEquals(WEB_APP_MANIFEST, mWebAppManifest);
    }
}
