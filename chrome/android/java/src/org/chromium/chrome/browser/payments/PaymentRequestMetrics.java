// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;

/**
 * A class used to record metrics for the Payment Request feature.
 */
public final class PaymentRequestMetrics {

    // PaymentRequestRequestedInformation defined in tools/metrics/histograms/histograms.xml.
    @VisibleForTesting
    public static final int REQUESTED_INFORMATION_NONE = 0;
    @VisibleForTesting
    public static final int REQUESTED_INFORMATION_EMAIL = 1 << 0;
    @VisibleForTesting
    public static final int REQUESTED_INFORMATION_PHONE = 1 << 1;
    @VisibleForTesting
    public static final int REQUESTED_INFORMATION_SHIPPING = 1 << 2;
    @VisibleForTesting
    public static final int REQUESTED_INFORMATION_MAX = 8;

    // There should be no instance of PaymentRequestMetrics created.
    private PaymentRequestMetrics() {}

    /*
     * Records the metric that keeps track of what user information are requested by merchants to
     * complete a payment request.
     *
     * @param requestEmail Whether the merchant requested an email address.
     * @param requestPhone Whether the merchant requested a phone number.
     * @param requestShipping Whether the merchant requested a shipping address.
     */
    public static void recordRequestedInformationHistogram(boolean requestEmail,
            boolean requestPhone, boolean requestShipping) {
        int requestInformation =
                (requestEmail ? REQUESTED_INFORMATION_EMAIL : 0)
                | (requestPhone ? REQUESTED_INFORMATION_PHONE : 0)
                | (requestShipping ? REQUESTED_INFORMATION_SHIPPING : 0);
        RecordHistogram.recordEnumeratedHistogram("PaymentRequest.RequestedInformation",
                requestInformation, REQUESTED_INFORMATION_MAX);
    }
}
