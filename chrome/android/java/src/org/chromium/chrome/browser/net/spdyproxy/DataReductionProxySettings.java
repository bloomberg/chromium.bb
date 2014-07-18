// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.net.spdyproxy;

import org.chromium.base.CalledByNative;
import org.chromium.base.ThreadUtils;

import java.text.NumberFormat;
import java.util.Locale;

/**
 * Entry point to manage all data reduction proxy configuration details.
 */
public class DataReductionProxySettings {

    /**
     * Data structure to hold the original content length before data reduction and the received
     * content length after data reduction.
     */
    public static class ContentLengths {
        private final long mOriginal;
        private final long mReceived;

        @CalledByNative("ContentLengths")
        public static ContentLengths create(long original, long received) {
            return new ContentLengths(original, received);
        }

        private ContentLengths(long original, long received) {
            mOriginal = original;
            mReceived = received;
        }

        public long getOriginal() {
            return mOriginal;
        }

        public long getReceived() {
            return mReceived;
        }
    }

    private static DataReductionProxySettings sSettings;

    /**
     * Returns a singleton instance of the settings object.
     */
    public static DataReductionProxySettings getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sSettings == null) {
            sSettings = new DataReductionProxySettings();
        }
        return sSettings;
    }

    private final long mNativeDataReductionProxySettings;

    private DataReductionProxySettings() {
        // Note that this technically leaks the native object, however,
        // DataReductionProxySettings is a singleton that lives forever and there's no clean
        // shutdown of Chrome on Android
        mNativeDataReductionProxySettings = nativeInit();
    }

    /**
     * Add a pattern for hosts to bypass. Wildcards should be compatible with the JavaScript
     * function shExpMatch, which can be used in proxy PAC resolution. This should be called
     * before the proxy is used.
     * @param pattern A pattern to match.
     */
    public void bypassHostPattern(String pattern) {
        nativeBypassHostPattern(mNativeDataReductionProxySettings, pattern);
    }

    /**
     * Add a pattern for URLs to bypass. Wildcards should be compatible with the JavaScript
     * function shExpMatch, which can be used in proxy PAC resolution. This should be called
     * before the proxy is used.
     * @param pattern A pattern to match.
     */
    public void bypassURLPattern(String pattern) {
        nativeBypassURLPattern(mNativeDataReductionProxySettings, pattern);
    }


    /** Returns true if the SPDY proxy is allowed to be used. */
    public boolean isDataReductionProxyAllowed() {
        return nativeIsDataReductionProxyAllowed(mNativeDataReductionProxySettings);
    }

    /** Returns true if the SPDY proxy promo is allowed to be shown. */
    public boolean isDataReductionProxyPromoAllowed() {
        return nativeIsDataReductionProxyPromoAllowed(mNativeDataReductionProxySettings);
    }

    /** Returns true if proxy alternative field trial is running. */
    public boolean isIncludedInAltFieldTrial() {
        return nativeIsIncludedInAltFieldTrial(mNativeDataReductionProxySettings);
    }

    /**
     * Returns the current data reduction proxy origin.
     */
    public String getDataReductionProxyOrigin() {
        return nativeGetDataReductionProxyOrigin(mNativeDataReductionProxySettings);
    }

    /**
     * Sets the preference on whether to enable/disable the SPDY proxy. This will zero out the
     * data reduction statistics if this is the first time the SPDY proxy has been enabled.
     */
    public void setDataReductionProxyEnabled(boolean enabled) {
        nativeSetDataReductionProxyEnabled(mNativeDataReductionProxySettings, enabled);
    }

    /** Returns true if the SPDY proxy is enabled. */
    public boolean isDataReductionProxyEnabled() {
        return nativeIsDataReductionProxyEnabled(mNativeDataReductionProxySettings);
    }

    /** Returns true if the SPDY proxy is managed by an administrator's policy. */
    public boolean isDataReductionProxyManaged() {
        return nativeIsDataReductionProxyManaged(mNativeDataReductionProxySettings);
    }

    /**
     * Returns the time that the data reduction statistics were last updated.
     * @return The last update time in milliseconds since the epoch.
     */
    public long getDataReductionLastUpdateTime()  {
        return nativeGetDataReductionLastUpdateTime(mNativeDataReductionProxySettings);
    }

    /**
     * Returns aggregate original and received content lengths.
     * @return The content lengths.
     */
    public ContentLengths getContentLengths() {
        return nativeGetContentLengths(mNativeDataReductionProxySettings);
    }

    /**
     * Retrieves the history of daily totals of bytes that would have been
     * received if no data reducing mechanism had been applied.
     * @return The history of daily totals
     */
    public long[] getOriginalNetworkStatsHistory() {
        return nativeGetDailyOriginalContentLengths(mNativeDataReductionProxySettings);
    }

    /**
     * Retrieves the history of daily totals of bytes that were received after
     * applying a data reducing mechanism.
     * @return The history of daily totals
     */
    public long[] getReceivedNetworkStatsHistory() {
        return nativeGetDailyReceivedContentLengths(mNativeDataReductionProxySettings);
    }

    /**
     * Determines if the data reduction proxy is currently unreachable.
     * @return true if the data reduction proxy is unreachable.
     */
    public boolean isDataReductionProxyUnreachable() {
        return nativeIsDataReductionProxyUnreachable(mNativeDataReductionProxySettings);
    }

    /**
     * @return The data reduction settings as a string percentage.
     */
    public String getContentLengthPercentSavings() {
        ContentLengths length = getContentLengths();

        double savings = 0;
        if (length.getOriginal() > 0L  && length.getOriginal() > length.getReceived()) {
            savings = (length.getOriginal() - length.getReceived()) / (double) length.getOriginal();
        }
        NumberFormat percentageFormatter = NumberFormat.getPercentInstance(Locale.getDefault());
        return percentageFormatter.format(savings);
    }

    private native long nativeInit();
    private native void nativeBypassHostPattern(
            long nativeDataReductionProxySettingsAndroid, String pattern);
    private native void nativeBypassURLPattern(
            long nativeDataReductionProxySettingsAndroid, String pattern);
    private native boolean nativeIsDataReductionProxyAllowed(
            long nativeDataReductionProxySettingsAndroid);
    private native boolean nativeIsDataReductionProxyPromoAllowed(
            long nativeDataReductionProxySettingsAndroid);
    private native boolean nativeIsIncludedInAltFieldTrial(
            long nativeDataReductionProxySettingsAndroid);
    private native String nativeGetDataReductionProxyOrigin(
            long nativeDataReductionProxySettingsAndroid);
    private native boolean nativeIsDataReductionProxyEnabled(
            long nativeDataReductionProxySettingsAndroid);
    private native boolean nativeIsDataReductionProxyManaged(
            long nativeDataReductionProxySettingsAndroid);
    private native void nativeSetDataReductionProxyEnabled(
            long nativeDataReductionProxySettingsAndroid, boolean enabled);
    private native long nativeGetDataReductionLastUpdateTime(
            long nativeDataReductionProxySettingsAndroid);
    private native ContentLengths nativeGetContentLengths(
            long nativeDataReductionProxySettingsAndroid);
    private native long[] nativeGetDailyOriginalContentLengths(
            long nativeDataReductionProxySettingsAndroid);
    private native long[] nativeGetDailyReceivedContentLengths(
            long nativeDataReductionProxySettingsAndroid);
    private native boolean nativeIsDataReductionProxyUnreachable(
            long nativeDataReductionProxySettingsAndroid);
}
