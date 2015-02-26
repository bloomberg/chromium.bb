// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.preference.PreferenceManager;

import org.chromium.base.CommandLine;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.ChromeSwitches;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.preferences.NetworkPredictionOptions;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.bandwidth.BandwidthType;

/**
 * Reads, writes, and migrates preferences related to network usage and privacy.
 */
public class PrivacyPreferencesManager implements CrashReportingPermissionManager{

    static final String PREF_CRASH_DUMP_UPLOAD = "crash_dump_upload";
    static final String PREF_CRASH_DUMP_UPLOAD_NO_CELLULAR = "crash_dump_upload_no_cellular";
    private static final String PREF_NETWORK_PREDICTIONS = "network_predictions";
    private static final String PREF_BANDWIDTH_OLD = "prefetch_bandwidth";
    private static final String PREF_BANDWIDTH_NO_CELLULAR_OLD = "prefetch_bandwidth_no_cellular";
    private static final String ALLOW_PRERENDER_OLD = "allow_prefetch";

    private static PrivacyPreferencesManager sInstance;

    private final Context mContext;
    private final SharedPreferences mSharedPreferences;

    private boolean mCrashUploadingEnabled;
    private final String mCrashDumpNeverUpload;
    private final String mCrashDumpWifiOnlyUpload;
    private final String mCrashDumpAlwaysUpload;

    @VisibleForTesting
    PrivacyPreferencesManager(Context context) {
        mContext = context;
        mSharedPreferences = PreferenceManager.getDefaultSharedPreferences(context);
        mCrashUploadingEnabled = true;
        mCrashDumpNeverUpload = context.getString(R.string.crash_dump_never_upload_value);
        mCrashDumpWifiOnlyUpload = context.getString(R.string.crash_dump_only_with_wifi_value);
        mCrashDumpAlwaysUpload = context.getString(R.string.crash_dump_always_upload_value);
    }

    public static PrivacyPreferencesManager getInstance(Context context) {
        if (sInstance == null) {
            sInstance = new PrivacyPreferencesManager(context);
        }
        return sInstance;
    }

    /**
     * Returns the Crash Dump Upload preference value.
     * @return String value of the preference.
     */
    public String getPrefCrashDumpUploadPreference() {
        return mSharedPreferences.getString(PREF_CRASH_DUMP_UPLOAD,
                mCrashDumpNeverUpload);
    }

    /**
     * Migrate and delete old preferences.  Note that migration has to happen in Android-specific
     * code because we need to access ALLOW_PRERENDER sharedPreference.
     * TODO(bnc) https://crbug.com/394845. This change is planned for M38. After a year or so, it
     * would be worth considering removing this migration code (also removing accessors in
     * PrefServiceBridge and pref_service_bridge), and reverting to default for users
     * who had set preferences but have not used Chrome for a year. This change would be subject to
     * privacy review.
     */
    public void migrateNetworkPredictionPreferences() {
        PrefServiceBridge prefService = PrefServiceBridge.getInstance();

        // See if PREF_NETWORK_PREDICTIONS is an old boolean value.
        boolean predictionOptionIsBoolean = false;
        try {
            mSharedPreferences.getString(PREF_NETWORK_PREDICTIONS, "");
        } catch (ClassCastException ex) {
            predictionOptionIsBoolean = true;
        }

        // Nothing to do if the user or this migration code has already set the new
        // preference.
        if (!predictionOptionIsBoolean
                && prefService.networkPredictionOptionsHasUserSetting()) {
            return;
        }

        // Nothing to do if the old preferences are unset.
        if (!predictionOptionIsBoolean
                && !mSharedPreferences.contains(PREF_BANDWIDTH_OLD)
                && !mSharedPreferences.contains(PREF_BANDWIDTH_NO_CELLULAR_OLD)) {
            return;
        }

        // Migrate if the old preferences are at their default values.
        // (Note that for PREF_BANDWIDTH*, if the setting is default, then there is no way to tell
        // whether the user has set it.)
        final String prefBandwidthDefault = BandwidthType.PRERENDER_ON_WIFI.title();
        final String prefBandwidth =
                mSharedPreferences.getString(PREF_BANDWIDTH_OLD, prefBandwidthDefault);
        boolean prefBandwidthNoCellularDefault = true;
        boolean prefBandwidthNoCellular = mSharedPreferences.getBoolean(
                PREF_BANDWIDTH_NO_CELLULAR_OLD, prefBandwidthNoCellularDefault);

        if (!(prefBandwidthDefault.equals(prefBandwidth))
                || (prefBandwidthNoCellular != prefBandwidthNoCellularDefault)) {
            NetworkPredictionOptions newValue = NetworkPredictionOptions.DEFAULT;
            // Observe PREF_BANDWIDTH on mobile network capable devices.
            if (isMobileNetworkCapable()) {
                if (mSharedPreferences.contains(PREF_BANDWIDTH_OLD)) {
                    BandwidthType prefetchBandwidthTypePref = BandwidthType.getBandwidthFromTitle(
                            prefBandwidth);
                    if (BandwidthType.NEVER_PRERENDER.equals(prefetchBandwidthTypePref)) {
                        newValue = NetworkPredictionOptions.NETWORK_PREDICTION_NEVER;
                    } else if (BandwidthType.PRERENDER_ON_WIFI.equals(prefetchBandwidthTypePref)) {
                        newValue = NetworkPredictionOptions.NETWORK_PREDICTION_WIFI_ONLY;
                    } else if (BandwidthType.ALWAYS_PRERENDER.equals(prefetchBandwidthTypePref)) {
                        newValue = NetworkPredictionOptions.NETWORK_PREDICTION_ALWAYS;
                    }
                }
            // Observe PREF_BANDWIDTH_NO_CELLULAR on devices without mobile network.
            } else {
                if (mSharedPreferences.contains(PREF_BANDWIDTH_NO_CELLULAR_OLD)) {
                    if (prefBandwidthNoCellular) {
                        newValue = NetworkPredictionOptions.NETWORK_PREDICTION_WIFI_ONLY;
                    } else {
                        newValue = NetworkPredictionOptions.NETWORK_PREDICTION_NEVER;
                    }
                }
            }
            // But disable after all if kNetworkPredictionEnabled was disabled by the user.
            if (prefService.networkPredictionEnabledHasUserSetting()
                    && !prefService.getNetworkPredictionEnabledUserPrefValue()) {
                newValue = NetworkPredictionOptions.NETWORK_PREDICTION_NEVER;
            }
            // Save new value in Chrome PrefService.
            prefService.setNetworkPredictionOptions(newValue);
        }

        // Delete old sharedPreferences.
        SharedPreferences.Editor sharedPreferencesEditor = mSharedPreferences.edit();
        // Delete PREF_BANDWIDTH and PREF_BANDWIDTH_NO_CELLULAR: just migrated these options.
        if (mSharedPreferences.contains(PREF_BANDWIDTH_OLD)) {
            sharedPreferencesEditor.remove(PREF_BANDWIDTH_OLD);
        }
        if (mSharedPreferences.contains(PREF_BANDWIDTH_NO_CELLULAR_OLD)) {
            sharedPreferencesEditor.remove(PREF_BANDWIDTH_NO_CELLULAR_OLD);
        }
        // Also delete ALLOW_PRERENDER, which was updated based on PREF_BANDWIDTH[_NO_CELLULAR] and
        // network connectivity type, therefore does not carry additional information.
        if (mSharedPreferences.contains(ALLOW_PRERENDER_OLD)) {
            sharedPreferencesEditor.remove(ALLOW_PRERENDER_OLD);
        }
        // Delete bool PREF_NETWORK_PREDICTIONS so that string values can be stored. Note that this
        // SharedPreference carries no information, because it used to be overwritten by
        // kNetworkPredictionEnabled on startup, and now it is overwritten by
        // kNetworkPredictionOptions on startup.
        if (mSharedPreferences.contains(PREF_NETWORK_PREDICTIONS)) {
            sharedPreferencesEditor.remove(PREF_NETWORK_PREDICTIONS);
        }
        sharedPreferencesEditor.apply();
    }

    private NetworkInfo getActiveNetworkInfo() {
        ConnectivityManager connectivityManager =
                (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        return connectivityManager.getActiveNetworkInfo();
    }

    protected boolean isNetworkAvailable() {
        NetworkInfo networkInfo = getActiveNetworkInfo();
        return (networkInfo != null && networkInfo.isConnected());
    }

    protected boolean isWiFiOrEthernetNetwork() {
        NetworkInfo networkInfo = getActiveNetworkInfo();
        return networkInfo != null
                && (networkInfo.getType() == ConnectivityManager.TYPE_WIFI
                        || networkInfo.getType() == ConnectivityManager.TYPE_ETHERNET);
    }

    protected boolean isMobileNetworkCapable() {
        return ((ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE))
            .getNetworkInfo(ConnectivityManager.TYPE_MOBILE) != null;
    }

    /**
     * Checks whether prerender should be allowed and updates the preference if it is not set yet.
     * @return Whether prerendering should be allowed.
     */
    public boolean shouldPrerender() {
        if (!DeviceClassManager.enablePrerendering()) return false;
        migrateNetworkPredictionPreferences();
        return PrefServiceBridge.getInstance().canPredictNetworkActions();
    }

    /**
     * Check whether to allow uploading crash dump. The option should be either
     * "always upload", or "wifi only" with current connection being wifi/ethernet.
     *
     * @return boolean to whether to allow uploading crash dump.
     */
    private boolean allowUploadCrashDump() {
        if (!isMobileNetworkCapable()) {
            return mSharedPreferences.getBoolean(PREF_CRASH_DUMP_UPLOAD_NO_CELLULAR, false);
        } else {
            String option =
                    mSharedPreferences.getString(PREF_CRASH_DUMP_UPLOAD, mCrashDumpNeverUpload);
            return option.equals(mCrashDumpAlwaysUpload)
                    || (option.equals(mCrashDumpWifiOnlyUpload) && isWiFiOrEthernetNetwork());
        }
    }

    /**
     * Sets the crash upload preference, which determines whether crash dumps will be uploaded
     * always, never, or only on wifi.
     *
     * @param when A String denoting when crash dump uploading is allowed. One of
     *             R.array.crash_upload_values.
     */
    public void setUploadCrashDump(String when) {
        // Set the crash upload preference regardless of the current connection status.
        boolean canUpload = !when.equals(mCrashDumpNeverUpload);
        PrefServiceBridge.getInstance().setCrashReporting(canUpload);
    }

    /**
     * Provides a way to disable crash uploading entirely, regardless of the preferences.
     * Used by tests that trigger crashers intentionally, so these crashers are not uploaded.
     */
    public void disableCrashUploading() {
        mCrashUploadingEnabled = false;
    }

    /**
     * Check whether crash dump upload preference is set to NEVER only.
     *
     * @return boolean {@code true} if the option is set to NEVER
     */
    public boolean isNeverUploadCrashDump() {
        boolean option;
        if (isMobileNetworkCapable()) {
            option = mSharedPreferences.getString(PREF_CRASH_DUMP_UPLOAD, mCrashDumpNeverUpload)
                    .equals(mCrashDumpNeverUpload);
        } else {
            option = !mSharedPreferences.getBoolean(PREF_CRASH_DUMP_UPLOAD_NO_CELLULAR, false);
        }
        return option;
    }

    /**
     * Sets the initial value for whether crash stacks may be uploaded.
     * This should be called only once, the first time Chrome is launched.
     */
    public void initCrashUploadPreference(boolean allowCrashUpload) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        if (isMobileNetworkCapable()) {
            if (allowCrashUpload) {
                ed.putString(PREF_CRASH_DUMP_UPLOAD, mCrashDumpWifiOnlyUpload);
            } else {
                ed.putString(PREF_CRASH_DUMP_UPLOAD, mCrashDumpNeverUpload);
            }
        } else {
            ed.putString(PREF_CRASH_DUMP_UPLOAD, mCrashDumpNeverUpload);
            ed.putBoolean(PREF_CRASH_DUMP_UPLOAD_NO_CELLULAR, allowCrashUpload);
        }
        ed.apply();
        PrefServiceBridge.getInstance().setCrashReporting(allowCrashUpload);
    }

    /**
     * Check whether to allow uploading crash dump now.
     * {@link #allowUploadCrashDump()} should return {@code true},
     * and the network should be connected as well.
     *
     * @return boolean to whether to allow uploading crash dump now.
     */
    @Override
    public boolean isUploadPermitted() {
        return mCrashUploadingEnabled && isNetworkAvailable() && (allowUploadCrashDump()
                || CommandLine.getInstance().hasSwitch(ChromeSwitches.FORCE_CRASH_DUMP_UPLOAD));
    }
}
