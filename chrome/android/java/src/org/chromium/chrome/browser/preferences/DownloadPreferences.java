// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.support.annotation.Nullable;

import org.chromium.chrome.R;

/**
 * Fragment to keep track of all downloads related preferences.
 */
public class DownloadPreferences extends PreferenceFragment {
    private static final String PREF_LOCATION_CHANGE = "location_change";
    private static final int REQUEST_CODE_LOCATION_CHANGE = 1;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getActivity().setTitle(R.string.menu_downloads);
        PreferenceUtils.addPreferencesFromResource(this, R.xml.download_preferences);

        Preference locationChangePref = (Preference) findPreference(PREF_LOCATION_CHANGE);
        locationChangePref.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
            @Override
            public boolean onPreferenceClick(Preference preference) {
                // TODO(jming): Implement actual directory selector.
                return false;
            }
        });

        updateSummaries();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQUEST_CODE_LOCATION_CHANGE && resultCode == Activity.RESULT_OK) {
            if (data == null || data.getData() == null) return;
            // TODO(jming): Update download default directory in preferences.
            updateSummaries();
        }
    }

    public void updateSummaries() {
        PrefServiceBridge prefServiceBridge = PrefServiceBridge.getInstance();

        Preference locationChangePref = (Preference) findPreference(PREF_LOCATION_CHANGE);
        if (locationChangePref != null) {
            locationChangePref.setSummary(prefServiceBridge.getDownloadDefaultDirectory());
        }
    }
}
