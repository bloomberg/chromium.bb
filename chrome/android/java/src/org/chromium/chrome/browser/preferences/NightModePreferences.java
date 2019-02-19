// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import static org.chromium.chrome.browser.preferences.ChromePreferenceManager.NIGHT_MODE_SETTINGS_ENABLED_KEY;

import android.os.Bundle;
import android.preference.PreferenceFragment;
import android.support.annotation.Nullable;

import org.chromium.chrome.R;

/**
 * Fragment to manage the night mode user settings.
 */
public class NightModePreferences extends PreferenceFragment {
    static final String PREF_NIGHT_MODE_SWITCH = "night_mode_switch";

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        PreferenceUtils.addPreferencesFromResource(this, R.xml.night_mode_preferences);
        // TODO(huayinz): Change to a proper string.
        getActivity().setTitle("Dark mode");

        // TODO(huayinz): Change this to a proper setting.
        ChromeSwitchPreference switchPreference =
                (ChromeSwitchPreference) findPreference(PREF_NIGHT_MODE_SWITCH);
        switchPreference.setChecked(ChromePreferenceManager.getInstance().readBoolean(
                NIGHT_MODE_SETTINGS_ENABLED_KEY, false));
        switchPreference.setOnPreferenceChangeListener((preference, newValue) -> {
            boolean enabled = (boolean) newValue;
            ChromePreferenceManager.getInstance().writeBoolean(
                    NIGHT_MODE_SETTINGS_ENABLED_KEY, enabled);
            return true;
        });
    }
}
