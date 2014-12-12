// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesTest;
import org.chromium.chrome.shell.ChromeShellTestBase;

/**
 * Tests for the "Save Passwords" settings screen.
 */
public class SavedPasswordsPreferencesTest extends ChromeShellTestBase {

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        startChromeBrowserProcessSync(getInstrumentation().getTargetContext());
    }

    /**
     * Ensure that the on/off switch in "Save Passwords" settings actually enables and disables
     * password saving.
     */
    @SmallTest
    @Feature({"Preferences"})
    public void testSavePasswordsSwitch() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge.getInstance().setRememberPasswordsEnabled(true);
            }
        });

        final Preferences preferences = PreferencesTest.startPreferences(getInstrumentation(),
                ManageSavedPasswordsPreferences.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ManageSavedPasswordsPreferences savedPasswordPrefs =
                        (ManageSavedPasswordsPreferences) preferences.getFragmentForTest();
                ChromeSwitchPreference onOffSwitch = (ChromeSwitchPreference)
                        savedPasswordPrefs.findPreference(
                                ManageSavedPasswordsPreferences.PREF_SAVE_PASSWORDS_SWITCH);
                assertTrue(onOffSwitch.isChecked());

                PreferencesTest.clickPreference(savedPasswordPrefs, onOffSwitch);
                assertFalse(PrefServiceBridge.getInstance().isRememberPasswordsEnabled());
                PreferencesTest.clickPreference(savedPasswordPrefs, onOffSwitch);
                assertTrue(PrefServiceBridge.getInstance().isRememberPasswordsEnabled());

                preferences.finish();

                PrefServiceBridge.getInstance().setRememberPasswordsEnabled(false);
            }
        });

        final Preferences preferences2 = PreferencesTest.startPreferences(getInstrumentation(),
                ManageSavedPasswordsPreferences.class.getName());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ManageSavedPasswordsPreferences savedPasswordPrefs =
                        (ManageSavedPasswordsPreferences) preferences2.getFragmentForTest();
                ChromeSwitchPreference onOffSwitch = (ChromeSwitchPreference)
                        savedPasswordPrefs.findPreference(
                                ManageSavedPasswordsPreferences.PREF_SAVE_PASSWORDS_SWITCH);
                assertFalse(onOffSwitch.isChecked());
            }
        });
    }
}
