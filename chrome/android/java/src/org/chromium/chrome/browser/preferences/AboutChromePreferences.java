// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PrefServiceBridge.ProfilePathCallback;

import java.util.Calendar;

/**
 * Settings fragment that displays information about Chrome.
 */
public class AboutChromePreferences extends PreferenceFragment implements
        ProfilePathCallback {

    private static final String PREF_APPLICATION_VERSION = "application_version";
    private static final String PREF_OS_VERSION = "os_version";
    private static final String PREF_WEBKIT_VERSION = "webkit_version";
    private static final String PREF_JAVASCRIPT_VERSION = "javascript_version";
    private static final String PREF_EXECUTABLE_PATH = "executable_path";
    private static final String PREF_PROFILE_PATH = "profile_path";
    private static final String PREF_LEGAL_INFORMATION = "legal_information";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getActivity().setTitle(R.string.prefs_about_chrome);
        addPreferencesFromResource(R.xml.about_chrome_preferences);

        PrefServiceBridge prefServiceBridge = PrefServiceBridge.getInstance();
        PrefServiceBridge.AboutVersionStrings versionStrings =
                prefServiceBridge.getAboutVersionStrings();
        Preference p = findPreference(PREF_APPLICATION_VERSION);
        p.setSummary(versionStrings.getApplicationVersion());
        p = findPreference(PREF_OS_VERSION);
        p.setSummary(versionStrings.getOSVersion());
        p = findPreference(PREF_WEBKIT_VERSION);
        p.setSummary(versionStrings.getWebkitVersion());
        p = findPreference(PREF_JAVASCRIPT_VERSION);
        p.setSummary(versionStrings.getJavascriptVersion());
        p = findPreference(PREF_EXECUTABLE_PATH);
        p.setSummary(getActivity().getPackageCodePath());
        p = findPreference(PREF_LEGAL_INFORMATION);
        int currentYear = Calendar.getInstance().get(Calendar.YEAR);
        p.setSummary(getString(R.string.legal_information_summary, currentYear));

        prefServiceBridge.getProfilePath(this);
    }

    @Override
    public void onGotProfilePath(String profilePath) {
        Preference pref = findPreference(PREF_PROFILE_PATH);
        if (pref != null) pref.setSummary(profilePath);
    }
}
