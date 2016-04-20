// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.settings;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.blimp.BrowserRestartActivity;
import org.chromium.blimp.R;
import org.chromium.blimp.preferences.PreferencesUtil;

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

        setAppVersion(getActivity());
        setOSVersion();
        setEngineIPandVersion(getActivity().getIntent());
        setupAssignerPreferences();
    }

    private void setAppVersion(Activity activity) {
        PackageManager pm = activity.getPackageManager();
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
    }

    private void setOSVersion() {
        Preference p = findPreference(PREF_OS_VERSION);
        p.setSummary("Android " + Build.VERSION.RELEASE);
    }

    private void setEngineIPandVersion(Intent intent) {
        String engineIP = intent.getStringExtra(EXTRA_ENGINE_IP);
        Preference p = findPreference(PREF_ENGINE_IP);
        p.setSummary(engineIP == null ? "" : engineIP);

        String engineVersion = intent.getStringExtra(EXTRA_ENGINE_VERSION);
        p = findPreference(PREF_ENGINE_VERSION);
        p.setSummary(engineVersion == null ? "" : engineVersion);
    }

    /**
     * When the user taps on the current assigner, a list of available assigners pops up.
     * User is allowed to change the assigner which is saved to shared preferences.
     * A dialog is displayed which prompts the user to restart the application.
     */
    private void setupAssignerPreferences() {
        final Activity activity = getActivity();
        String assigner = PreferencesUtil.getLastUsedAssigner(activity);

        final ListPreference lp = (ListPreference) findPreference(PREF_ASSIGNER_URL);
        lp.setSummary(assigner == null ? "" : assigner);

        lp.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                String newAssignmentUrl = (String) newValue;
                lp.setSummary(newAssignmentUrl);
                lp.setValue(newAssignmentUrl);

                PreferencesUtil.setLastUsedAssigner(activity, newAssignmentUrl);
                showRestartDialog(activity);

                return true;
            }
        });
    }

    private void showRestartDialog(final Context context) {
        // TODO(shaktisahu): Change this to use android.support.v7.app.AlertDialog later.
        new AlertDialog.Builder(context)
                .setTitle(R.string.restart_blimp)
                .setMessage(R.string.blimp_assigner_changed_please_restart)
                .setPositiveButton(R.string.restart_now,
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                restartBrowser(context);
                            }
                        })
                .create()
                .show();
    }

    private void restartBrowser(Context context) {
        Intent intent = BrowserRestartActivity.createRestartIntent(context);
        context.startActivity(intent);
    }
}
