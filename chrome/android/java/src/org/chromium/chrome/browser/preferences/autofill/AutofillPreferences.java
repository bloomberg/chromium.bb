// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.autofill;

import android.app.Activity;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceFragment;
import android.preference.PreferenceGroup;
import android.preference.PreferenceScreen;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;

/**
 * Autofill settings fragment, which allows the user to edit autofill and credit card profiles.
 */
public class AutofillPreferences extends PreferenceFragment
        implements OnPreferenceChangeListener, PersonalDataManager.PersonalDataManagerObserver {

    public static final String AUTOFILL_GUID = "guid";
    // Need to be in sync with kSettingsOrigin[] in
    // chrome/browser/ui/webui/options/autofill_options_handler.cc
    public static final String SETTINGS_ORIGIN = "Chrome settings";
    private static final int SUMMARY_CHAR_LENGTH = 40;
    private static final String PREF_AUTOFILL_SWITCH = "autofill_switch";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.autofill_preferences);
        getActivity().setTitle(R.string.prefs_autofill);

        ChromeSwitchPreference autofillSwitch =
                (ChromeSwitchPreference) findPreference(PREF_AUTOFILL_SWITCH);

        boolean isAutofillEnabled = PersonalDataManager.isAutofillEnabled();
        autofillSwitch.setChecked(isAutofillEnabled);

        autofillSwitch.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                PersonalDataManager.setAutofillEnabled((boolean) newValue);
                return true;
            }
        });

    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        rebuildLists();
        return true;
    }

    /**
     * Rebuild all the profile and credit card lists.
     */
    private void rebuildLists() {
        rebuildProfileList();
        rebuildCreditCardList();
    }

    // Always clears the list before building/rebuilding.
    private void rebuildProfileList() {
        // Add an edit preference for each current Chrome profile.
        PreferenceGroup profileCategory = (PreferenceGroup) findPreference("autofill_profiles");
        profileCategory.removeAll();
        for (AutofillProfile profile : PersonalDataManager.getInstance().getProfiles()) {
            // Add an item on the current page...
            PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getActivity());
            screen.setTitle(profile.getFullName());

            // TODO(aruslan): I18N and new UI style: http://crbug.com/178541.
            String summary = profile.getLabel();
            if (summary.length() > SUMMARY_CHAR_LENGTH) {
                summary = summary.substring(0, SUMMARY_CHAR_LENGTH);
                summary += "...";
            }
            screen.setSummary(summary);

            screen.setFragment(AutofillProfileEditor.class.getName());
            Bundle args = screen.getExtras();
            args.putString(AUTOFILL_GUID, profile.getGUID());
            profileCategory.addPreference(screen);
        }
    }

    private void rebuildCreditCardList() {
        PreferenceGroup profileCategory = (PreferenceGroup) findPreference("autofill_credit_cards");
        profileCategory.removeAll();
        for (CreditCard card : PersonalDataManager.getInstance().getCreditCards()) {
            // Add an item on the current page...
            PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getActivity());
            screen.setTitle(card.getName());
            screen.setSummary(card.getObfuscatedNumber());
            screen.setFragment(AutofillCreditCardEditor.class.getName());
            Bundle args = screen.getExtras();
            args.putString(AUTOFILL_GUID, card.getGUID());
            profileCategory.addPreference(screen);
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        // Always rebuild our list of profiles.  Although we could
        // detect if profiles are added or deleted (GUID list
        // changes), the profile summary (name+addr) might be
        // different.  To be safe, we update all.
        rebuildLists();
    }

    @Override
    public void onPersonalDataChanged() {
        rebuildLists();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        PersonalDataManager.getInstance().unregisterDataObserver(this);
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);
        PersonalDataManager.getInstance().registerDataObserver(this);
    }
}
