// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.bandwidth;

import org.chromium.base.metrics.RecordHistogram;

/**
 * Centralizes UMA data collection for the Data Reduction Proxy.
 */
public class DataReductionProxyUma {
    // Represent the possible user actions in the settings menu. This must remain in
    // sync with DataReductionProxy.UIAction in tools/metrics/histograms/histograms.xml.
    public static final int ACTION_ENABLED = 0;
    // The value of 1 is reserved for an iOS-specific action.
    public static final int ACTION_SETTINGS_LINK_ENABLED = 2;
    public static final int ACTION_SETTINGS_LINK_NOT_ENABLED = 3;
    public static final int ACTION_DISMISSED = 4;
    public static final int ACTION_OFF_TO_OFF = 5;
    public static final int ACTION_OFF_TO_ON = 6;
    public static final int ACTION_ON_TO_OFF = 7;
    public static final int ACTION_ON_TO_ON = 8;
    public static final int ACTION_INDEX_BOUNDARY = 9;

    /**
     * Record the DataReductionProxy.UIAction histogram.
     * @param action User action at the promo or settings screen
     */
    public static void dataReductionProxyUIAction(int action) {
        assert action >= 0 && action < ACTION_INDEX_BOUNDARY;
        RecordHistogram.recordEnumeratedHistogram(
                "DataReductionProxy.UIAction", action,
                DataReductionProxyUma.ACTION_INDEX_BOUNDARY);
    }
}