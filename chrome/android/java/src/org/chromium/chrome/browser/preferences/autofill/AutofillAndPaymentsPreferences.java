// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.autofill;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceFragment;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;

/**
 * Autofill and payments settings fragment, which allows the user to edit autofill and credit card
 * profiles and control payment apps.
 */
public class AutofillAndPaymentsPreferences extends PreferenceFragment {
    public static final String AUTOFILL_GUID = "guid";

    // Needs to be in sync with kSettingsOrigin[] in
    // chrome/browser/ui/webui/options/autofill_options_handler.cc
    public static final String SETTINGS_ORIGIN = "Chrome settings";
    private static final String PREF_AUTOFILL_SWITCH = "autofill_switch";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.autofill_and_payments_preferences);
        getActivity().setTitle(R.string.prefs_autofill_and_payments);

        ChromeSwitchPreference autofillSwitch =
                (ChromeSwitchPreference) findPreference(PREF_AUTOFILL_SWITCH);
        autofillSwitch.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                PersonalDataManager.setAutofillEnabled((boolean) newValue);
                return true;
            }
        });
    }

    @Override
    public void onResume() {
        super.onResume();
        ((ChromeSwitchPreference) findPreference(PREF_AUTOFILL_SWITCH))
                .setChecked(PersonalDataManager.isAutofillEnabled());
    }
}
