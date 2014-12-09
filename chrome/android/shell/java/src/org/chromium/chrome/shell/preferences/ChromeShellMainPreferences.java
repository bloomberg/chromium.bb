// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell.preferences;

import android.os.Bundle;
import android.preference.PreferenceFragment;

import org.chromium.chrome.shell.R;

/**
 * The main settings fragment for Chrome Shell.
 */
public class ChromeShellMainPreferences extends PreferenceFragment {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.chrome_shell_main_preferences);
    }
}