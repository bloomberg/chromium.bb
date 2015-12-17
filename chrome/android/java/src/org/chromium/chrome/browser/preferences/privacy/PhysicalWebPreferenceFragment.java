// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceFragment;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;

/**
 * Fragment to manage the Physical Web preference and to explain to the user what it does.
 */
public class PhysicalWebPreferenceFragment extends PreferenceFragment {

    private static final String PREF_PHYSICAL_WEB_SWITCH = "physical_web_switch";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.physical_web_preferences);
        getActivity().setTitle(R.string.physical_web_pref_title);
        initPhysicalWebSwitch();
    }

    private void initPhysicalWebSwitch() {
        ChromeSwitchPreference physicalWebSwitch =
                (ChromeSwitchPreference) findPreference(PREF_PHYSICAL_WEB_SWITCH);

        boolean isPhysicalWebEnabled =
                PrivacyPreferencesManager.getInstance(getActivity()).isPhysicalWebEnabled();
        physicalWebSwitch.setChecked(isPhysicalWebEnabled);

        physicalWebSwitch.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                PrivacyPreferencesManager.getInstance(getActivity()).setPhysicalWebEnabled(
                        (boolean) newValue);
                return true;
            }
        });
    }

}