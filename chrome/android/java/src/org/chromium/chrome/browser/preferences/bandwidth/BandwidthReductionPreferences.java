// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.bandwidth;

import static org.chromium.third_party.android.datausagechart.ChartDataUsageView.DAYS_IN_CHART;

import android.content.Context;
import android.content.res.Resources;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceFragment;
import android.preference.PreferenceManager;
import android.text.format.DateUtils;

import org.chromium.base.CommandLine;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.infobar.DataReductionProxyInfoBar;
import org.chromium.chrome.browser.metrics.UmaBridge;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.ManagedPreferenceDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.third_party.android.datausagechart.NetworkStats;
import org.chromium.third_party.android.datausagechart.NetworkStatsHistory;

/**
 * Settings fragment that allows the user to configure Data Saver.
 */
public class BandwidthReductionPreferences extends PreferenceFragment {

    private static final String PREF_REDUCE_DATA_USAGE_STATISTICS =
            "data_usage_statistics_category";
    public static final String PREF_REDUCE_DATA_USAGE_SWITCH = "reduce_data_usage_switch";

    private static final String SHARED_PREF_DISPLAYED_INFOBAR = "displayed_data_reduction_infobar";

    // This is the same as Chromium data_reduction_proxy::switches::kEnableDataReductionProxy.
    private static final String ENABLE_DATA_REDUCTION_PROXY = "enable-spdy-proxy-auth";

    private boolean mIsEnabled;
    private boolean mWasEnabledAtCreation;
    // Whether the current activity is started from the Data Reduction Promo.
    private boolean mFromPromo;
    // State transitions for data reduction settings. These correspond to
    // DataReductionProxy.SettingsConversion defined in
    // /tools/histograms/histograms.xml.
    public static final int DATA_REDUCTION_OFF_TO_OFF = 0;
    public static final int DATA_REDUCTION_OFF_TO_ON = 1;
    public static final int DATA_REDUCTION_ON_TO_OFF = 2;
    public static final int DATA_REDUCTION_ON_TO_ON = 3;
    public static final int DATA_REDUCTION_INDEX_BOUNDARY = 4;

    // Static flags to track if the first enable/disable for the session
    // has happened, independent of instantiations of this class.
    private static boolean sProxyTurnedOnThisSession = false;
    private static boolean sProxyTurnedOffThisSession = false;
    private static Object sProxyLock = new Object();

    public static void launchDataReductionSSLInfoBar(Context context, WebContents webContents) {
        // The infobar is displayed if the Chrome instance is part of the SSL experiment field
        // trial, the FRE has completed, the proxy is enabled, and the infobar has not been
        // displayed before.
        if (!DataReductionProxySettings.getInstance().isIncludedInAltFieldTrial()) return;
        if (!DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) return;
        if (DataReductionProxySettings.getInstance().isDataReductionProxyManaged()) return;
        if (!FirstRunStatus.getFirstRunFlowComplete(context)) return;
        if (getDisplayedDataReductionInfoBar(context)) return;
        DataReductionProxyInfoBar.launch(webContents,
                context.getString(R.string.data_reduction_infobar_text),
                context.getString(R.string.learn_more),
                context.getString(R.string.data_reduction_experiment_link_url));
        setDisplayedDataReductionInfoBar(context, true);
    }

    private void toggleProxyTurnOn(boolean value) {
        synchronized (sProxyLock) {
            sProxyTurnedOnThisSession = value;
        }
    }

    private void toggleProxyTurnOff(boolean value) {
        synchronized (sProxyLock) {
            sProxyTurnedOffThisSession = value;
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        addPreferencesFromResource(R.xml.bandwidth_reduction_preferences);
        getActivity().setTitle(R.string.reduce_data_usage_title);
        boolean isEnabled =
                DataReductionProxySettings.getInstance().isDataReductionProxyEnabled();
        mIsEnabled = !isEnabled;
        mWasEnabledAtCreation = isEnabled;
        updatePreferences(isEnabled);

        setHasOptionsMenu(true);

        Bundle extras = getActivity().getIntent().getExtras();
        mFromPromo = (extras != null) && extras.getBoolean(DataReductionPromoScreen.FROM_PROMO);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        int statusChange = DATA_REDUCTION_OFF_TO_OFF;
        if (mWasEnabledAtCreation) {
            statusChange = mIsEnabled ? DATA_REDUCTION_ON_TO_ON : DATA_REDUCTION_ON_TO_OFF;
        } else {
            statusChange = mIsEnabled ? DATA_REDUCTION_OFF_TO_ON : DATA_REDUCTION_OFF_TO_OFF;
        }
        UmaBridge.dataReductionProxySettings(statusChange);
        if (mFromPromo && statusChange == DATA_REDUCTION_OFF_TO_ON) {
            UmaBridge.dataReductionProxyTurnedOnFromPromo();
        }
        if (statusChange == DATA_REDUCTION_OFF_TO_ON && !sProxyTurnedOnThisSession) {
            toggleProxyTurnOn(true);
            UmaBridge.dataReductionProxyTurnedOn();
        }
        if (statusChange == DATA_REDUCTION_ON_TO_OFF && !sProxyTurnedOffThisSession) {
            toggleProxyTurnOff(true);
            UmaBridge.dataReductionProxyTurnedOff();
        }
    }

    /**
     * Switches preference screens depending on whether bandwidth reduction is enabled/disabled.
     * @param isEnabled Indicates whether bandwidth reduction is enabled.
     */
    public void updatePreferences(boolean isEnabled) {
        if (mIsEnabled == isEnabled) return;
        getPreferenceScreen().removeAll();
        createReduceDataUsageSwitch(isEnabled);
        if (isEnabled) {
            addPreferencesFromResource(R.xml.bandwidth_reduction_preferences);
            updateReductionStatistics();
        } else {
            addPreferencesFromResource(R.xml.bandwidth_reduction_preferences_off);
            if (!DataReductionProxySettings.getInstance().isIncludedInAltFieldTrial()) {
                getPreferenceScreen().removePreference(
                        findPreference("data_reduction_experiment_text"));
                getPreferenceScreen().removePreference(
                        findPreference("data_reduction_experiment_link"));
            }
        }
        mIsEnabled = isEnabled;
    }

    /**
     * Updates the preference screen to convey current statistics on bandwidth reduction.
     */
    public void updateReductionStatistics() {
        DataReductionProxySettings config = DataReductionProxySettings.getInstance();

        BandwidthStatisticsPreferenceCategory bandwidthStatsPref =
                (BandwidthStatisticsPreferenceCategory)
                getPreferenceScreen().findPreference(PREF_REDUCE_DATA_USAGE_STATISTICS);
        long original[] = config.getOriginalNetworkStatsHistory();
        long received[] = config.getReceivedNetworkStatsHistory();
        bandwidthStatsPref.setReductionStats(
                config.getDataReductionLastUpdateTime(),
                getNetworkStatsHistory(original, DAYS_IN_CHART),
                getNetworkStatsHistory(received, DAYS_IN_CHART));
    }

    /**
     * Returns summary string.
     */
    public static String generateSummary(Resources resources) {
        if (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) {
            String percent = DataReductionProxySettings.getInstance()
                    .getContentLengthPercentSavings();
            return resources.getString(
                    R.string.data_reduction_menu_item_summary, percent);
        } else {
            return (String) resources.getText(R.string.text_off);
        }
    }

    private static NetworkStatsHistory getNetworkStatsHistory(long[] history, int days) {
        if (days > history.length) days = history.length;
        NetworkStatsHistory networkStatsHistory =
                new NetworkStatsHistory(
                        DateUtils.DAY_IN_MILLIS, days, NetworkStatsHistory.FIELD_RX_BYTES);

        DataReductionProxySettings config = DataReductionProxySettings.getInstance();
        long time = config.getDataReductionLastUpdateTime() - days * DateUtils.DAY_IN_MILLIS;
        for (int i = history.length - days, bucket = 0; i < history.length; i++, bucket++) {
            NetworkStats.Entry entry = new NetworkStats.Entry();
            entry.rxBytes = history[i];
            long startTime = time + (DateUtils.DAY_IN_MILLIS * bucket);
            // Spread each day's record over the first hour of the day.
            networkStatsHistory.recordData(
                    startTime, startTime + DateUtils.HOUR_IN_MILLIS, entry);
        }
        return networkStatsHistory;
    }

    private void createReduceDataUsageSwitch(boolean isEnabled) {
        final ChromeSwitchPreference reduceDataUsageSwitch =
                new ChromeSwitchPreference(getActivity(), null);
        reduceDataUsageSwitch.setKey(PREF_REDUCE_DATA_USAGE_SWITCH);
        reduceDataUsageSwitch.setSummaryOn(R.string.text_on);
        reduceDataUsageSwitch.setSummaryOff(R.string.text_off);
        reduceDataUsageSwitch.setDrawDivider(true);
        reduceDataUsageSwitch.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                        reduceDataUsageSwitch.getContext(), (boolean) newValue);
                BandwidthReductionPreferences.this.updatePreferences((boolean) newValue);
                return true;
            }
        });
        reduceDataUsageSwitch.setManagedPreferenceDelegate(new ManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return CommandLine.getInstance().hasSwitch(ENABLE_DATA_REDUCTION_PROXY)
                        || DataReductionProxySettings.getInstance().isDataReductionProxyManaged();
            }
        });

        getPreferenceScreen().addPreference(reduceDataUsageSwitch);

        // Note: setting the switch state before the preference is added to the screen results in
        // some odd behavior where the switch state doesn't always match the internal enabled state
        // (e.g. the switch will say "On" when reduced data usage is really turned off), so
        // .setChecked() should be called after .addPreference()
        reduceDataUsageSwitch.setChecked(isEnabled);
    }

    private static boolean getDisplayedDataReductionInfoBar(Context context) {
        return PreferenceManager.getDefaultSharedPreferences(context).getBoolean(
                SHARED_PREF_DISPLAYED_INFOBAR, false);
    }

    private static void setDisplayedDataReductionInfoBar(Context context, boolean displayed) {
        PreferenceManager.getDefaultSharedPreferences(context).edit()
                .putBoolean(SHARED_PREF_DISPLAYED_INFOBAR, displayed)
                .apply();
    }
}
