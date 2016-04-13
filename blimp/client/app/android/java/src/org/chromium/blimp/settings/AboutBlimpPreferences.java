// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.settings;

import android.app.Activity;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.blimp.R;

/**
 * Fragment to display blimp client and engine version related info.
 */
public class AboutBlimpPreferences extends PreferenceFragment {
    private static final String TAG = "AboutBlimp";
    public static final String EXTRA_ENGINE_IP = "engine_ip";
    public static final String EXTRA_ENGINE_VERSION = "engine_version";
    public static final String EXTRA_ASSIGNER_URL = "assigner_url";

    private static final String PREF_APPLICATION_VERSION = "application_version";
    private static final String PREF_OS_VERSION = "os_version";
    private static final String PREF_ENGINE_IP = "blimp_engine_ip";
    private static final String PREF_ENGINE_VERSION = "blimp_engine_version";
    private static final String PREF_ASSIGNER_URL = "blimp_assigner_url";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getActivity().setTitle(R.string.about_blimp_preferences);
        addPreferencesFromResource(R.xml.about_blimp_preferences);

        Activity activity = getActivity();
        PackageManager pm = getActivity().getPackageManager();
        try {
            ApplicationInfo applicationInfo = pm.getApplicationInfo(activity.getPackageName(), 0);
            PackageInfo packageInfo = pm.getPackageInfo(activity.getPackageName(), 0);

            String versionString;
            if (!TextUtils.isEmpty(packageInfo.versionName)
                    && Character.isDigit(packageInfo.versionName.charAt(0))) {
                versionString = activity.getString(
                        R.string.subtitle_for_version_number, packageInfo.versionName);
            } else {
                versionString = packageInfo.versionName;
            }

            Preference p = findPreference(PREF_APPLICATION_VERSION);
            p.setSummary(versionString);
        } catch (PackageManager.NameNotFoundException e) {
            Log.d(TAG, "Fetching ApplicationInfo failed.", e);
        }

        Preference p = findPreference(PREF_OS_VERSION);
        p.setSummary("Android " + Build.VERSION.RELEASE);

        String engineIP = getActivity().getIntent().getStringExtra(EXTRA_ENGINE_IP);
        p = findPreference(PREF_ENGINE_IP);
        p.setSummary(engineIP == null ? "" : engineIP);

        String engineVersion = getActivity().getIntent().getStringExtra(EXTRA_ENGINE_VERSION);
        p = findPreference(PREF_ENGINE_VERSION);
        p.setSummary(engineVersion == null ? "" : engineVersion);

        String assigner = getActivity().getIntent().getStringExtra(EXTRA_ASSIGNER_URL);
        p = findPreference(PREF_ASSIGNER_URL);
        p.setSummary(assigner == null ? "" : assigner);
    }
}
