// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceFragment;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Settings fragment that allows the user to configure content suggestions.
 */
public class ContentSuggestionsPreferences extends PreferenceFragment {
    private static final String PREF_MAIN_SWITCH = "suggestions_switch";
    private static final String PREF_NOTIFICATIONS_SWITCH = "suggestions_notifications_switch";
    private static final String PREF_CAVEATS = "suggestions_caveats";
    private static final String PREF_LEARN_MORE = "suggestions_learn_more";

    private boolean mIsEnabled;

    // Preferences, modified as the state of the screen changes.
    private ChromeSwitchPreference mFeatureSwitch;
    private ChromeSwitchPreference mNotificationsSwitch;
    private Preference mCaveatsDescription;
    private Preference mLearnMoreButton;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        addPreferencesFromResource(R.xml.suggestions_preferences);
        setHasOptionsMenu(true);
        finishSwitchInitialisation();

        boolean isEnabled = SnippetsBridge.isRemoteSuggestionsServiceEnabled();
        mIsEnabled = !isEnabled; // Opposite so that we trigger side effects below.
        updatePreferences(isEnabled);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(R.drawable.ic_help_and_feedback);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            // TODO(dgn): The help page needs to be added and the context reserved.
            HelpAndFeedback.getInstance(getActivity())
                    .show(getActivity(), getString(R.string.help_context_suggestions),
                            Profile.getLastUsedProfile(), null);
            return true;
        }
        return false;
    }

    /**
     * Switches preference screens depending on whether the remote suggestions are enabled/disabled.
     * @param isEnabled Indicates whether the remote suggestions are enabled.
     */
    public void updatePreferences(boolean isEnabled) {
        if (mIsEnabled == isEnabled) return;

        mFeatureSwitch.setChecked(isEnabled);

        if (isEnabled) {
            setNotificationsPrefState(true);
            setCaveatsPrefState(false);
        } else {
            setNotificationsPrefState(false);
            setCaveatsPrefState(true);
        }
        mIsEnabled = isEnabled;
    }

    private void setNotificationsPrefState(boolean visible) {
        if (visible) {
            if (findPreference(PREF_NOTIFICATIONS_SWITCH) != null) return;
            getPreferenceScreen().addPreference(mNotificationsSwitch);
            mNotificationsSwitch.setChecked(true); // TODO(dgn): initialise properly.
        } else {
            getPreferenceScreen().removePreference(mNotificationsSwitch);
        }
    }

    private void setCaveatsPrefState(boolean visible) {
        if (visible) {
            if (findPreference(PREF_CAVEATS) != null) return;
            getPreferenceScreen().addPreference(mCaveatsDescription);
            getPreferenceScreen().addPreference(mLearnMoreButton);
        } else {
            getPreferenceScreen().removePreference(mCaveatsDescription);
            getPreferenceScreen().removePreference(mLearnMoreButton);
        }
    }

    private void finishSwitchInitialisation() {
        mFeatureSwitch = (ChromeSwitchPreference) findPreference(PREF_MAIN_SWITCH);
        mNotificationsSwitch = (ChromeSwitchPreference) findPreference(PREF_NOTIFICATIONS_SWITCH);
        mCaveatsDescription = findPreference(PREF_CAVEATS);
        mLearnMoreButton = findPreference(PREF_LEARN_MORE);

        mFeatureSwitch.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                SnippetsBridge.setRemoteSuggestionsServiceEnabled((boolean) newValue);
                // TODO(dgn): Is there a way to have a visual feedback of when the remote
                // suggestions service has completed being turned on or off?
                ContentSuggestionsPreferences.this.updatePreferences((boolean) newValue);
                return true;
            }
        });
        mFeatureSwitch.setManagedPreferenceDelegate(new ManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return SnippetsBridge.isRemoteSuggestionsServiceManaged();
            }

            @Override
            public boolean isPreferenceControlledByCustodian(Preference preference) {
                return SnippetsBridge.isRemoteSuggestionsServiceManagedByCustodian();
            }
        });

        mNotificationsSwitch.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                // TODO(dgn) implement preference change.
                return true;
            }
        });
    }
}
