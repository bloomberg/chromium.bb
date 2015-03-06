// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.net.spdyproxy;

import android.content.Context;
import android.preference.PreferenceManager;

import org.chromium.base.CalledByNative;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;

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

    private static final String ENABLED_PREFERENCE_TAG = "BANDWIDTH_REDUCTION_PROXY_ENABLED";

    /** Returns whether the data reduction proxy is enabled.
     *
     * The knowledge of the data reduction proxy status is needed before the
     * native library is loaded.
     *
     * Note that the returned value can be out-of-date if the Data Reduction
     * Proxy is enabled/disabled from the native side without going through the
     * UI. The discrepancy will however be fixed at the next launch, so the
     * value returned here can be wrong (both false-positive and false-negative)
     * right after such a change.
     *
     * @param context The application context.
     * @return Whether the data reduction proxy is enabled.
     */
    public static boolean isEnabledBeforeNativeLoad(Context context) {
        // TODO(lizeb): Add a listener for the native preference change to keep
        // both in sync and avoid the false-positives and false-negatives.
        return PreferenceManager.getDefaultSharedPreferences(context).getBoolean(
            ENABLED_PREFERENCE_TAG, false);
    }

    /** Initializes DataReductionProxySettings.
     *
     * This method must be called before getInstance().
     *
     * @param context The application context.
     */
    @SuppressFBWarnings("LI_LAZY_INIT")
    public static void initialize(Context context) {
        ThreadUtils.assertOnUiThread();
        if (sSettings == null) {
            sSettings = new DataReductionProxySettings();
            boolean enabled = sSettings.isDataReductionProxyEnabled();
            PreferenceManager.getDefaultSharedPreferences(context).edit()
                .putBoolean(ENABLED_PREFERENCE_TAG, enabled).apply();
        }
    }

    /**
     * Returns a singleton instance of the settings object.
     */
    public static DataReductionProxySettings getInstance() {
        ThreadUtils.assertOnUiThread();
        assert sSettings != null : "initialize() must be called first.";
        return sSettings;
    }

    private final long mNativeDataReductionProxySettings;

    private DataReductionProxySettings() {
        // Note that this technically leaks the native object, however,
        // DataReductionProxySettings is a singleton that lives forever and there's no clean
        // shutdown of Chrome on Android
        mNativeDataReductionProxySettings = nativeInit();
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
     * Sets the preference on whether to enable/disable the SPDY proxy. This will zero out the
     * data reduction statistics if this is the first time the SPDY proxy has been enabled.
     */
    public void setDataReductionProxyEnabled(Context context, boolean enabled) {
        PreferenceManager.getDefaultSharedPreferences(context).edit()
                .putBoolean(ENABLED_PREFERENCE_TAG, enabled).apply();
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
    private native boolean nativeIsDataReductionProxyAllowed(
            long nativeDataReductionProxySettingsAndroid);
    private native boolean nativeIsDataReductionProxyPromoAllowed(
            long nativeDataReductionProxySettingsAndroid);
    private native boolean nativeIsIncludedInAltFieldTrial(
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
