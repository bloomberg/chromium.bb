// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.util.ApplicationData;
import org.chromium.content.browser.test.NativeLibraryTestBase;

/**
 * Tests for the PaymentRequestMetrics class.
 */
public class PaymentRequestMetricsUnitTest extends NativeLibraryTestBase {

    @Override
    public void setUp() throws Exception {
        super.setUp();
        ApplicationData.clearAppData(getInstrumentation().getTargetContext());
        loadNativeLibraryAndInitBrowserProcess();
    }

    /*
     * Tests that logs to PaymentRequest.RequestedInformation are made to the right enum bucket
     * depending on the information requested by the merchant.
     */
    @SmallTest
    @Feature({"Payments"})
    public void testRequestedInformationMetrics() {
        // No information requested.
        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.RequestedInformation",
                PaymentRequestMetrics.REQUESTED_INFORMATION_NONE));
        PaymentRequestMetrics.recordRequestedInformationHistogram(false, false, false);
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.RequestedInformation",
                PaymentRequestMetrics.REQUESTED_INFORMATION_NONE));

        // Only email requested.
        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.RequestedInformation",
                PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL));
        PaymentRequestMetrics.recordRequestedInformationHistogram(true, false, false);
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.RequestedInformation",
                PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL));

        // Only phone requested.
        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.RequestedInformation",
                PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE));
        PaymentRequestMetrics.recordRequestedInformationHistogram(false, true, false);
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.RequestedInformation",
                PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE));

        // Only shipping address requested.
        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.RequestedInformation",
                PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING));
        PaymentRequestMetrics.recordRequestedInformationHistogram(false, false, true);
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.RequestedInformation",
                PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING));

        // Email and phone requested.
        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.RequestedInformation",
                PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                | PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE));
        PaymentRequestMetrics.recordRequestedInformationHistogram(true, true, false);
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.RequestedInformation",
                PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                | PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE));

        // Email and shipping address requested.
        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.RequestedInformation",
                PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING));
        PaymentRequestMetrics.recordRequestedInformationHistogram(true, false, true);
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.RequestedInformation",
                PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING));

        // Phone and shipping address requested.
        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.RequestedInformation",
                PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING));
        PaymentRequestMetrics.recordRequestedInformationHistogram(false, true, true);
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.RequestedInformation",
                PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING));

        // Email, phone and shipping address requested.
        assertEquals(0, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.RequestedInformation",
                PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                | PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING));
        PaymentRequestMetrics.recordRequestedInformationHistogram(true, true, true);
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                "PaymentRequest.RequestedInformation",
                PaymentRequestMetrics.REQUESTED_INFORMATION_EMAIL
                | PaymentRequestMetrics.REQUESTED_INFORMATION_PHONE
                | PaymentRequestMetrics.REQUESTED_INFORMATION_SHIPPING));
    }
}
