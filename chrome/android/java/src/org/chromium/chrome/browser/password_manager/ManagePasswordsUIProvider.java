// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import android.app.Activity;

import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.password.SavePasswordsPreferences;

/**
 * Launches an activity presenting the Settings > Passwords UI.
 */
public class ManagePasswordsUIProvider {
    /**
     * Mehod called from the main settings to show the UI to manage passwords.
     *
     * @param activity the activity from which to launch the settings page.
     */
    public void showManagePasswordsUI(Activity activity) {
        if (activity == null) return;
        // Launch preference activity with SavePasswordsPreferences fragment.
        PreferencesLauncher.launchSettingsPage(activity, SavePasswordsPreferences.class);
    }
}
