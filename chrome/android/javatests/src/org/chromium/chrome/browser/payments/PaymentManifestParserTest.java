// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
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
import org.chromium.components.payments.PaymentManifestParser;
import org.chromium.components.payments.PaymentManifestParser.ManifestParseCallback;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.payments.mojom.WebAppManifestSection;

import java.net.URI;

/** An integration test for the payment manifest parser. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
public class PaymentManifestParserTest implements ManifestParseCallback {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private final PaymentManifestParser mParser = new PaymentManifestParser();
    private URI[] mWebAppManifestUris;
    private WebAppManifestSection[] mWebAppManifest;
    private boolean mParseFailure;
    private boolean mParsePaymentMethodManifestSuccess;
    private boolean mParseWebAppManifestSuccess;

    @Override
    public void onPaymentMethodManifestParseSuccess(URI[] webAppManifestUris) {
        mParsePaymentMethodManifestSuccess = true;
        mWebAppManifestUris = webAppManifestUris.clone();
    }

    @Override
    public void onWebAppManifestParseSuccess(WebAppManifestSection[] manifest) {
        mParseWebAppManifestSuccess = true;
        mWebAppManifest = manifest.clone();
    }

    @Override
    public void onManifestParseFailure() {
        mParseFailure = true;
    }

    @Before
    public void setUp() throws Throwable {
        mRule.startMainActivityOnBlankPage();
        mRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mParser.startUtilityProcess();
            }
        });
        mWebAppManifestUris = null;
        mWebAppManifest = null;
        mParseFailure = false;
        mParsePaymentMethodManifestSuccess = false;
        mParseWebAppManifestSuccess = false;
    }

    @After
    public void tearDown() throws Throwable {
        mRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mParser.stopUtilityProcess();
            }
        });
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testParseInvalidPaymentMethodManifest() throws Throwable {
        mRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mParser.parsePaymentMethodManifest(
                        "invalid payment method manifest", PaymentManifestParserTest.this);
            }
        });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mParseFailure;
            }
        });
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testParsePaymentMethodManifest() throws Throwable {
        mRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mParser.parsePaymentMethodManifest("{"
                                + "  \"default_applications\": ["
                                + "    \"https://bobpay.com/app.json\","
                                + "    \"https://alicepay.com/app.json\""
                                + "  ],"
                                + "  \"supported_origins\": ["
                                + "    \"https://charliepay.com\","
                                + "    \"https://evepay.com\""
                                + "  ]"
                                + "}",
                        PaymentManifestParserTest.this);
            }
        });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mParsePaymentMethodManifestSuccess;
            }
        });
        Assert.assertNotNull(mWebAppManifestUris);
        Assert.assertEquals(2, mWebAppManifestUris.length);
        Assert.assertEquals(new URI("https://bobpay.com/app.json"), mWebAppManifestUris[0]);
        Assert.assertEquals(new URI("https://alicepay.com/app.json"), mWebAppManifestUris[1]);
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testParseInvalidWebAppManifest() throws Throwable {
        mRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mParser.parseWebAppManifest(
                        "invalid web app manifest", PaymentManifestParserTest.this);
            }
        });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mParseFailure;
            }
        });
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testParseWebAppManifest() throws Throwable {
        mRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mParser.parseWebAppManifest("{"
                                + "  \"related_applications\": [{"
                                + "    \"platform\": \"play\", "
                                + "    \"id\": \"com.bobpay.app\", "
                                + "    \"min_version\": \"1\", "
                                + "    \"fingerprints\": [{"
                                + "      \"type\": \"sha256_cert\", "
                                + "      \"value\": \""
                                + "00:01:02:03:04:05:06:07:08:09:"
                                + "A0:A1:A2:A3:A4:A5:A6:A7:A8:A9:"
                                + "B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
                                + "    }]"
                                + "  }]"
                                + "}",
                        PaymentManifestParserTest.this);
            }
        });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mParseWebAppManifestSuccess;
            }
        });
        Assert.assertNotNull(mWebAppManifest);
        Assert.assertEquals(1, mWebAppManifest.length);
        Assert.assertNotNull(mWebAppManifest[0]);
        Assert.assertEquals("com.bobpay.app", mWebAppManifest[0].id);
        Assert.assertEquals(1, mWebAppManifest[0].minVersion);
        Assert.assertNotNull(mWebAppManifest[0].fingerprints);
        Assert.assertEquals(1, mWebAppManifest[0].fingerprints.length);
        Assert.assertNotNull(mWebAppManifest[0].fingerprints[0]);
        Assert.assertEquals(32, mWebAppManifest[0].fingerprints[0].length);
        Assert.assertArrayEquals(
                new byte[] {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, (byte) 0xA0,
                        (byte) 0xA1, (byte) 0xA2, (byte) 0xA3, (byte) 0xA4, (byte) 0xA5,
                        (byte) 0xA6, (byte) 0xA7, (byte) 0xA8, (byte) 0xA9, (byte) 0xB0,
                        (byte) 0xB1, (byte) 0xB2, (byte) 0xB3, (byte) 0xB4, (byte) 0xB5,
                        (byte) 0xB6, (byte) 0xB7, (byte) 0xB8, (byte) 0xB9, (byte) 0xC0,
                        (byte) 0xC1},
                mWebAppManifest[0].fingerprints[0]);
    }
}