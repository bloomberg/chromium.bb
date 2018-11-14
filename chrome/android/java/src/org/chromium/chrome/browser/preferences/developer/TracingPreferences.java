// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.developer;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PreferenceUtils;
import org.chromium.chrome.browser.tracing.TracingController;
import org.chromium.chrome.browser.tracing.TracingNotificationManager;

/**
 * Settings fragment that shows options for recording a performance trace.
 */
public class TracingPreferences extends PreferenceFragment implements TracingController.Observer {
    @VisibleForTesting
    static final String UI_PREF_START_RECORDING = "start_recording";
    @VisibleForTesting
    static final String UI_PREF_TRACING_STATUS = "tracing_status";

    private Preference mPrefStartRecording;
    private Preference mPrefTracingStatus;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getActivity().setTitle(R.string.prefs_tracing);
        PreferenceUtils.addPreferencesFromResource(this, R.xml.tracing_preferences);

        mPrefStartRecording = findPreference(UI_PREF_START_RECORDING);
        mPrefTracingStatus = findPreference(UI_PREF_TRACING_STATUS);

        mPrefStartRecording.setOnPreferenceClickListener(preference -> {
            TracingController.getInstance().startRecording();
            updatePreferences();
            return true;
        });
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferences();
        TracingController.getInstance().addObserver(this);
    }

    @Override
    public void onPause() {
        super.onPause();
        TracingController.getInstance().removeObserver(this);
    }

    @Override
    public void onTracingStateChanged(@TracingController.State int state) {
        updatePreferences();
    }

    private void updatePreferences() {
        @TracingController.State
        int state = TracingController.getInstance().getState();
        boolean initialized = state != TracingController.State.INITIALIZING;
        boolean idle = state == TracingController.State.IDLE || !initialized;
        boolean notificationsEnabled = TracingNotificationManager.browserNotificationsEnabled();

        mPrefStartRecording.setEnabled(idle && initialized && notificationsEnabled);

        if (!notificationsEnabled) {
            mPrefStartRecording.setTitle(R.string.prefs_tracing_start);
            mPrefTracingStatus.setTitle(R.string.tracing_notifications_disabled);
        } else if (idle) {
            mPrefStartRecording.setTitle(R.string.prefs_tracing_start);
            mPrefTracingStatus.setTitle(R.string.prefs_tracing_privacy_notice);
        } else {
            mPrefStartRecording.setTitle(R.string.prefs_tracing_active);
            mPrefTracingStatus.setTitle(R.string.prefs_tracing_active_summary);
        }
    }
}
