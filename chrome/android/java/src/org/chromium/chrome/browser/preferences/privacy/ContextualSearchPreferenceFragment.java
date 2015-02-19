// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceFragment;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.ManagedPreferenceDelegate;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;

/**
 * Fragment to manage the Contextual Search preference and to explain to the user what it does.
 */
public class ContextualSearchPreferenceFragment extends PreferenceFragment {

    private static final String PREF_CONTEXTUAL_SEARCH_SWITCH = "contextual_search_switch";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.contextual_search_preferences);
        getActivity().setTitle(R.string.contextual_search_title);
        setHasOptionsMenu(true);
        initContextualSearchSwitch();
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        menu.add(Menu.NONE, R.id.menu_id_contextual_search_learn, Menu.NONE, R.string.learn_more);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() != R.id.menu_id_contextual_search_learn) {
            return false;
        }

        ((Preferences) getActivity()).showUrl(R.string.learn_more,
                R.string.contextual_search_learn_more_url);
        return true;
    }

    private void initContextualSearchSwitch() {
        ChromeSwitchPreference contextualSearchSwitch =
                (ChromeSwitchPreference) findPreference(PREF_CONTEXTUAL_SEARCH_SWITCH);

        boolean isContextualSearchEnabled =
                !PrefServiceBridge.getInstance().isContextualSearchDisabled();
        contextualSearchSwitch.setChecked(isContextualSearchEnabled);

        contextualSearchSwitch.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                PrefServiceBridge.getInstance().setContextualSearchState((boolean) newValue);
                ((Preferences) getActivity()).logContextualSearchToggled((boolean) newValue);
                return true;
            }
        });
        contextualSearchSwitch.setManagedPreferenceDelegate(new ManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return PrefServiceBridge.getInstance().isContextualSearchDisabledByPolicy();
            }
        });
    }

}
