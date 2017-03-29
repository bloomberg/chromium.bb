// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.util.ApplicationData;
import org.chromium.content.browser.test.NativeLibraryTestRule;

/**
 * Tests for the PaymentRequestMetrics class.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PaymentRequestMetricsUnitTest {
    @Rule
    public NativeLibraryTestRule mActivityTestRule = new NativeLibraryTestRule();

    @Before
    public void setUp() throws Exception {
        ApplicationData.clearAppData(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        mActivityTestRule.loadNativeLibraryAndInitBrowserProcess();
    }

    /*
     * Tests that logs to PaymentRequest.RequestedInformation are made to the right enum bucket
     * depending on the information requested by the merchant.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testRequestedInformationMetrics() {
        // No information requested.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_NONE));
        PaymentRequestMetrics.recordRequestedInformationHistogram(false, false, false, false);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_NONE));

        // Only email requested.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL));
        PaymentRequestMetrics.recordRequestedInformationHistogram(true, false, false, false);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL));

        // Only phone requested.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE));
        PaymentRequestMetrics.recordRequestedInformationHistogram(false, true, false, false);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE));

        // Only shipping address requested.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING));
        PaymentRequestMetrics.recordRequestedInformationHistogram(false, false, true, false);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING));

        // Email and phone requested.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE));
        PaymentRequestMetrics.recordRequestedInformationHistogram(true, true, false, false);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE));

        // Email and shipping address requested.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING));
        PaymentRequestMetrics.recordRequestedInformationHistogram(true, false, true, false);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING));

        // Phone and shipping address requested.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING));
        PaymentRequestMetrics.recordRequestedInformationHistogram(false, true, true, false);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING));

        // Email, phone and shipping address requested.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING));
        PaymentRequestMetrics.recordRequestedInformationHistogram(true, true, true, false);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING));

        // Name requested.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_NAME));
        PaymentRequestMetrics.recordRequestedInformationHistogram(false, false, false, true);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_NAME));

        // Email and name requested.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_NAME));
        PaymentRequestMetrics.recordRequestedInformationHistogram(true, false, false, true);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_NAME));

        // Phone and name requested.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_NAME));
        PaymentRequestMetrics.recordRequestedInformationHistogram(false, true, false, true);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_NAME));

        // Email, phone and name requested.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_NAME));
        PaymentRequestMetrics.recordRequestedInformationHistogram(true, true, false, true);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_NAME));

        // Shipping address and name requested.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_NAME));
        PaymentRequestMetrics.recordRequestedInformationHistogram(false, false, true, true);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_NAME));

        // Email, shipping address and name requested.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_NAME));
        PaymentRequestMetrics.recordRequestedInformationHistogram(true, false, true, true);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_NAME));

        // Phone shipping address and name requested.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_NAME));
        PaymentRequestMetrics.recordRequestedInformationHistogram(false, true, true, true);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_NAME));

        // Email, phone, shipping address and name requested.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_NAME));
        PaymentRequestMetrics.recordRequestedInformationHistogram(true, true, true, true);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.RequestedInformation",
                        PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING
                                | PaymentRequestMetrics.REQUESTED_INFORMATION_NAME));
    }
}
