// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.download;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.support.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromeBasePreference;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferenceUtils;

/**
 * Fragment to keep track of all downloads related preferences.
 */
public class DownloadPreferences
        extends PreferenceFragment implements Preference.OnPreferenceChangeListener {
    private static final String PREF_LOCATION_CHANGE = "location_change";
    private static final String PREF_LOCATION_PROMPT_ENABLED = "location_prompt_enabled";

    private ChromeBasePreference mLocationChangePref;
    private ChromeSwitchPreference mLocationPromptEnabledPref;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getActivity().setTitle(R.string.menu_downloads);
        PreferenceUtils.addPreferencesFromResource(this, R.xml.download_preferences);

        mLocationPromptEnabledPref =
                (ChromeSwitchPreference) findPreference(PREF_LOCATION_PROMPT_ENABLED);
        mLocationPromptEnabledPref.setOnPreferenceChangeListener(this);

        mLocationChangePref = (ChromeBasePreference) findPreference(PREF_LOCATION_CHANGE);

        updateSummaries();
    }

    @Override
    public void onResume() {
        super.onResume();
        updateSummaries();
    }

    private void updateSummaries() {
        if (mLocationChangePref != null) {
            mLocationChangePref.setSummary(
                    PrefServiceBridge.getInstance().getDownloadDefaultDirectory());
        }

        if (mLocationPromptEnabledPref != null) {
            boolean isLocationPromptEnabled =
                    PrefServiceBridge.getInstance().getBoolean(Pref.PROMPT_FOR_DOWNLOAD_ANDROID);
            mLocationPromptEnabledPref.setChecked(isLocationPromptEnabled);
        }
    }

    // Preference.OnPreferenceChangeListener implementation.

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (PREF_LOCATION_PROMPT_ENABLED.equals(preference.getKey())) {
            PrefServiceBridge.getInstance().setBoolean(
                    Pref.PROMPT_FOR_DOWNLOAD_ANDROID, (boolean) newValue);
        }
        return true;
    }
}
