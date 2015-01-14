// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.autofill;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceFragment;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.preferences.ButtonPreference;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;

/**
 * Autofill Wallet integration fragment, which allows the user to control import of cards and
 * addresses from their Wallet.
 */
public class AutofillWalletPreferences extends PreferenceFragment {
    private static final String PREF_AUTOFILL_WALLET_SWITCH = "autofill_wallet_switch";
    private static final String PREF_AUTOFILL_CLEAR_UNMASKED_CARDS =
            "autofill_clear_unmasked_cards";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.autofill_wallet_preferences);
        getActivity().setTitle(R.string.autofill_wallet_title);

        ChromeSwitchPreference walletSwitch =
                (ChromeSwitchPreference) findPreference(PREF_AUTOFILL_WALLET_SWITCH);
        walletSwitch.setChecked(PersonalDataManager.isWalletImportEnabled());
        walletSwitch.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                PersonalDataManager.setWalletImportEnabled((boolean) newValue);
                return true;
            }
        });

        ButtonPreference clearUnmaskedCards =
                (ButtonPreference) findPreference(PREF_AUTOFILL_CLEAR_UNMASKED_CARDS);
        clearUnmaskedCards.setOnPreferenceClickListener(new OnPreferenceClickListener() {
            @Override
            public boolean onPreferenceClick(Preference preference) {
                PersonalDataManager.getInstance().clearUnmaskedCache();
                return true;
            }
        });
    }
}
