// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import org.chromium.chrome.browser.preferences.bandwidth.BandwidthReductionPreferences;
import org.chromium.chrome.browser.preferences.bandwidth.DataReductionPromoScreen;

/**
 * Static methods to record user actions.
 *
 * We have a different native method for each action as we want to use c++ code to
 * extract user command keys from code and show them in the dashboard.
 * See: chromium/src/content/browser/user_metrics.h for more details.
 */
public class UmaBridge {

    /**
     * Record that the user opened the menu.
     */
    public static void menuShow() {
        nativeRecordMenuShow();
    }

    /**
     * Record that the user opened the menu through software menu button.
     * @param isByHwButton
     * @param isDragging
     */
    public static void usingMenu(boolean isByHwButton, boolean isDragging) {
        nativeRecordUsingMenu(isByHwButton, isDragging);
    }

    // Android beam

    public static void beamCallbackSuccess() {
        nativeRecordBeamCallbackSuccess();
    }

    public static void beamInvalidAppState() {
        nativeRecordBeamInvalidAppState();
    }

    // Data Saver

    /**
     * Record that Data Saver was turned on.
     */
    public static void dataReductionProxyTurnedOn() {
        nativeRecordDataReductionProxyTurnedOn();
    }

    /**
     * Record that Data Saver was turned off.
     */
    public static void dataReductionProxyTurnedOff() {
        nativeRecordDataReductionProxyTurnedOff();
    }

    /**
     * Record that Data Saver was turned on immediately after the user viewed the promo screen.
     */
    public static void dataReductionProxyTurnedOnFromPromo() {
        nativeRecordDataReductionProxyTurnedOnFromPromo();
    }

    /**
     * Record the DataReductionProxy.PromoAction histogram.
     * @param action User action at the promo screen
     */
    public static void dataReductionProxyPromoAction(int action) {
        assert action >= 0 && action < DataReductionPromoScreen.ACTION_INDEX_BOUNDARY;
        nativeRecordDataReductionProxyPromoAction(
                action, DataReductionPromoScreen.ACTION_INDEX_BOUNDARY);
    }

    /**
     * Record that the Data Saver promo was displayed.
     */
    public static void dataReductionProxyPromoDisplayed() {
        nativeRecordDataReductionProxyPromoDisplayed();
    }

    /**
     * Record the DataReductionProxy.SettingsConversion histogram.
     * @param statusChange ON/OFF change at the data saver setting menu
     */
    public static void dataReductionProxySettings(int statusChange) {
        assert statusChange >= 0
                && statusChange < BandwidthReductionPreferences.DATA_REDUCTION_INDEX_BOUNDARY;
        nativeRecordDataReductionProxySettings(
                statusChange, BandwidthReductionPreferences.DATA_REDUCTION_INDEX_BOUNDARY);
    }

    private static native void nativeRecordMenuShow();
    private static native void nativeRecordUsingMenu(boolean isByHwButton, boolean isDragging);
    private static native void nativeRecordBeamInvalidAppState();
    private static native void nativeRecordBeamCallbackSuccess();
    private static native void nativeRecordDataReductionProxyTurnedOn();
    private static native void nativeRecordDataReductionProxyTurnedOff();
    private static native void nativeRecordDataReductionProxyTurnedOnFromPromo();
    private static native void nativeRecordDataReductionProxyPromoAction(int action, int boundary);
    private static native void nativeRecordDataReductionProxyPromoDisplayed();
    private static native void nativeRecordDataReductionProxySettings(int statusChange,
            int boundary);
}
