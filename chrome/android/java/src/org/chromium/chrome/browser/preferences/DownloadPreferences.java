// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.os.Bundle;
import android.os.Environment;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.support.annotation.Nullable;
import android.support.v4.content.ContextCompat;

import org.chromium.chrome.R;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/**
 * Fragment to keep track of all downloads related preferences.
 */
public class DownloadPreferences
        extends PreferenceFragment implements Preference.OnPreferenceChangeListener {
    private static final String PREF_LOCATION_CHANGE = "location_change";
    private static final String PREF_LOCATION_PROMPT_ENABLED = "location_prompt_enabled";
    private static final String OPTION_PRIMARY_STORAGE = "primary";

    private PrefServiceBridge mPrefServiceBridge;
    private ListPreference mLocationChangePref;
    private ChromeSwitchPreference mLocationPromptEnabledPref;

    private List<File> mSecondaryDirs = new ArrayList<>();

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mPrefServiceBridge = PrefServiceBridge.getInstance();

        getActivity().setTitle(R.string.menu_downloads);
        PreferenceUtils.addPreferencesFromResource(this, R.xml.download_preferences);

        mLocationPromptEnabledPref =
                (ChromeSwitchPreference) findPreference(PREF_LOCATION_PROMPT_ENABLED);
        mLocationPromptEnabledPref.setOnPreferenceChangeListener(this);

        mLocationChangePref = (ListPreference) findPreference(PREF_LOCATION_CHANGE);
        getLocationOptions();

        updateSummaries();
    }

    private void updateSummaries() {
        if (mLocationChangePref != null) {
            mLocationChangePref.setSummary(mPrefServiceBridge.getDownloadDefaultDirectory());
        }

        if (mLocationPromptEnabledPref != null) {
            boolean isLocationPromptEnabled =
                    mPrefServiceBridge.getBoolean(Pref.PROMPT_FOR_DOWNLOAD_ANDROID);
            mLocationPromptEnabledPref.setChecked(isLocationPromptEnabled);
        }
    }

    private void getLocationOptions() {
        File[] externalDirectories = ContextCompat.getExternalFilesDirs(
                getActivity().getApplicationContext(), Environment.DIRECTORY_DOWNLOADS);

        // Go through and remove all null reported directories.
        List<File> externalDirs = new ArrayList<>();
        for (File dir : externalDirectories) {
            if (dir != null) {
                externalDirs.add(dir);
            }
        }

        int numLocations = externalDirs.size();
        CharSequence[] values = new String[numLocations];
        CharSequence[] descriptions = new String[numLocations];

        // Default selected option is primary storage.
        values[0] = OPTION_PRIMARY_STORAGE;
        descriptions[0] = getActivity().getString(R.string.downloads_primary_storage);
        int selectedValueIndex = 0;

        // If there is only one directory option, make sure this is the selected option.
        if (externalDirs.size() == 1) {
            if (!getPrimaryDownloadDirectory().equals(
                        new File(mPrefServiceBridge.getDownloadDefaultDirectory()))) {
                mPrefServiceBridge.setDownloadDefaultDirectory(
                        getPrimaryDownloadDirectory().getAbsolutePath());
            }
        }

        for (File dir : externalDirs) {
            if (dir == null) continue;

            // Skip directory in primary storage.
            if (dir.getAbsolutePath().contains(
                        Environment.getExternalStorageDirectory().getAbsolutePath())) {
                continue;
            }

            mSecondaryDirs.add(dir);

            int index = mSecondaryDirs.size();
            values[index] = String.valueOf(index - 1); // actual index in mSecondaryDirs.
            descriptions[index] = getActivity().getString(R.string.downloads_secondary_storage);
            // Add index if there is more than one secondary storage option.
            if (index > 1) {
                descriptions[index] = descriptions[index] + " " + String.valueOf(index);
            }

            // Select this option if the directory matches stored default.
            if (dir.equals(new File(mPrefServiceBridge.getDownloadDefaultDirectory()))) {
                selectedValueIndex = index;
            }
        }

        mLocationChangePref.setEntryValues(values);
        mLocationChangePref.setEntries(descriptions);
        mLocationChangePref.setValueIndex(selectedValueIndex);
        mLocationChangePref.setOnPreferenceChangeListener(this);
    }

    // Preference.OnPreferenceChangeListener:

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (PREF_LOCATION_CHANGE.equals(preference.getKey())) {
            updateStorageLocation((String) newValue);
        } else if (PREF_LOCATION_PROMPT_ENABLED.equals(preference.getKey())) {
            mPrefServiceBridge.setBoolean(Pref.PROMPT_FOR_DOWNLOAD_ANDROID, (boolean) newValue);
        }
        return true;
    }

    private void updateStorageLocation(String newValue) {
        File directory = newValue.equals(OPTION_PRIMARY_STORAGE)
                ? getPrimaryDownloadDirectory()
                : mSecondaryDirs.get(Integer.parseInt(newValue));
        mPrefServiceBridge.setDownloadDefaultDirectory(directory.getAbsolutePath());

        updateSummaries();
    }

    /**
     * @return The default download directory for primary storage (often internal storage).
     */
    private File getPrimaryDownloadDirectory() {
        return Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
    }
}
