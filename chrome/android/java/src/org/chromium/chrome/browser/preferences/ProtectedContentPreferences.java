// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceFragment;
import android.widget.Toast;

import org.chromium.chrome.R;
import org.chromium.content.browser.MediaDrmCredentialManager;
import org.chromium.content.browser.MediaDrmCredentialManager.MediaDrmCredentialManagerCallback;

/**
 * Fragment to keep track of the protected content preferences.
 */
public class ProtectedContentPreferences extends PreferenceFragment implements
        ProtectedContentResetCredentialConfirmDialogFragment.Listener {

    private static final String PREF_RESET_DEVICE_CREDENTIALS_BUTTON =
            "reset_device_credentials_button";
    private static final String PREF_PROTECTED_CONTENT_SWITCH = "protected_content_switch";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.protected_content_preferences);
        getActivity().setTitle(R.string.protected_content);

        ChromeSwitchPreference protectedContentSwitch =
                (ChromeSwitchPreference) findPreference(PREF_PROTECTED_CONTENT_SWITCH);

        boolean isProtectedContentEnabled =
                PrefServiceBridge.getInstance().isProtectedMediaIdentifierEnabled();
        protectedContentSwitch.setChecked(isProtectedContentEnabled);

        protectedContentSwitch.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                PrefServiceBridge.getInstance().setProtectedMediaIdentifierEnabled(
                        (boolean) newValue);
                return true;
            }
        });


        ButtonPreference resetDeviceCredentialsButton = (ButtonPreference)
                findPreference(PREF_RESET_DEVICE_CREDENTIALS_BUTTON);

        resetDeviceCredentialsButton.setOnPreferenceClickListener(new OnPreferenceClickListener(){
            @Override
            public boolean onPreferenceClick(Preference preference) {
                new ProtectedContentResetCredentialConfirmDialogFragment(
                        ProtectedContentPreferences.this).show(getFragmentManager(), null);
                return true;
            }
        });
    }

    @Override
    public void resetDeviceCredential() {
        MediaDrmCredentialManager.resetCredentials(new MediaDrmCredentialManagerCallback() {
            @Override
            public void onCredentialResetFinished(boolean succeeded) {
                if (!succeeded) {
                    String message = getString(
                            R.string.protected_content_reset_failed);
                    Toast.makeText(getActivity(), message, Toast.LENGTH_SHORT).show();
                }
            }
        });
    }
}
