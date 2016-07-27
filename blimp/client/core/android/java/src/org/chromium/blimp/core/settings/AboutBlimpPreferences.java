// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.settings;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.support.v7.app.AlertDialog;

import org.chromium.base.VisibleForTesting;
import org.chromium.blimp.R;
import org.chromium.blimp_public.BlimpSettingsCallbacks;

/**
 * Blimp preferences page in Chrome.
 */
public class AboutBlimpPreferences extends PreferenceFragment {

    /**
     * Blimp switch preference key, also the key for this PreferenceFragment.
     */
    public static final String PREF_BLIMP_SWITCH = "blimp_switch";
    /**
     * Blimp assigner URL preference key.
     */
    public static final String PREF_ASSIGNER_URL = "blimp_assigner_url";

    private static BlimpSettingsCallbacks sSettingsCallback;

    /**
     * Attach the blimp setting preferences to a {@link PreferenceFragment}.
     * @param fragment The fragment that blimp setting attach to.
     */
    public static void addBlimpPreferences(PreferenceFragment fragment) {
        PreferenceScreen screen = fragment.getPreferenceScreen();

        Preference blimpSetting = new Preference(fragment.getActivity());
        blimpSetting.setTitle(R.string.blimp_about_blimp_preferences);
        blimpSetting.setFragment(AboutBlimpPreferences.class.getName());
        blimpSetting.setKey(AboutBlimpPreferences.PREF_BLIMP_SWITCH);

        screen.addPreference(blimpSetting);
        fragment.setPreferenceScreen(screen);
    }

    /**
     * Register Chrome callback related to Blimp settings.
     * @param callback
     */
    public static void registerCallback(BlimpSettingsCallbacks callback) {
        sSettingsCallback = callback;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getActivity().setTitle(R.string.blimp_about_blimp_preferences);
        addPreferencesFromResource(R.xml.blimp_preferences);

        setupBlimpSwitch();
        setupAssignerPreferences();
    }

    /**
     * Setup the switch preference for blimp.
     */
    private void setupBlimpSwitch() {
        // TODO(xingliu): Use {@link ChromeSwitchPreference} after move this class to Chrome.
        // http://crbug.com/630675
        final Preference pref = findPreference(PREF_BLIMP_SWITCH);

        pref.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                // For blimp client 0.6, if blimp switch changed, show restart dialog.
                // TODO(xingliu): Figure out if we need to close all tabs before restart or reset
                // the settings before restarting.
                showRestartDialog(getActivity(), R.string.blimp_switch_changed_please_restart);
                return true;
            }
        });
    }

    /**
     * When the user taps on the current assigner, a list of available assigners pops up.
     * User is allowed to change the assigner which is saved to shared preferences.
     * A dialog is displayed which prompts the user to restart the application.
     *
     * Use {@link PreferencesUtil#getLastUsedAssigner} to retrieve the assigner URL.
     */
    private void setupAssignerPreferences() {
        final Activity activity = getActivity();

        final ListPreference listPreference = (ListPreference) findPreference(PREF_ASSIGNER_URL);
        listPreference.setSummary(listPreference.getValue());

        listPreference.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                String newAssignmentUrl = (String) newValue;
                listPreference.setSummary(newAssignmentUrl);
                showRestartDialog(activity, R.string.blimp_assigner_changed_please_restart);
                return true;
            }
        });
    }

    /**
     * Show restart browser dialog.
     * @param context The context where we display the restart browser dialog.
     * @param message The message shown to the user.
     */
    private static void showRestartDialog(final Context context, int message) {
        new AlertDialog.Builder(context)
                .setTitle(R.string.blimp_restart_blimp)
                .setMessage(message)
                .setPositiveButton(R.string.blimp_restart_now,
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                restartBrowser();
                            }
                        })
                .create()
                .show();
    }

    /**
     * Restart the browser.
     */
    @VisibleForTesting
    protected static void restartBrowser() {
        if (sSettingsCallback != null) {
            sSettingsCallback.onRestartBrowserRequested();
        }
    }
}
