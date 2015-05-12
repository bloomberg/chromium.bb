// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.preferences.ChromeBaseCheckBoxPreference;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesTest;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.content.common.ContentSwitches;

/**
 * Tests for the "Save Passwords" settings screen.
 */
public class SavedPasswordsPreferencesTest extends ChromeShellTestBase {

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        startChromeBrowserProcessSync(getInstrumentation().getTargetContext());
        CommandLine.getInstance().appendSwitch(ContentSwitches.ENABLE_CREDENTIAL_MANAGER_API);
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

    /**
     * Ensure that the "Auto Sign-in" switch in "Save Passwords" settings actually enables and
     * disables auto sign-in.
     */
    @SmallTest
    @Feature({"Preferences"})
    public void testAutoSignInCheckbox() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge.getInstance().setPasswordManagerAutoSigninEnabled(true);
            }
        });

        final Preferences preferences = PreferencesTest.startPreferences(
                getInstrumentation(), ManageSavedPasswordsPreferences.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertTrue(CommandLine.getInstance().hasSwitch(
                        ContentSwitches.ENABLE_CREDENTIAL_MANAGER_API));
                ManageSavedPasswordsPreferences passwordPrefs =
                        (ManageSavedPasswordsPreferences) preferences.getFragmentForTest();
                ChromeBaseCheckBoxPreference onOffSwitch =
                        (ChromeBaseCheckBoxPreference) passwordPrefs.findPreference(
                                ManageSavedPasswordsPreferences.PREF_AUTOSIGNIN_SWITCH);
                assertTrue(onOffSwitch.isChecked());

                PreferencesTest.clickPreference(passwordPrefs, onOffSwitch);
                assertFalse(PrefServiceBridge.getInstance().isPasswordManagerAutoSigninEnabled());
                PreferencesTest.clickPreference(passwordPrefs, onOffSwitch);
                assertTrue(PrefServiceBridge.getInstance().isPasswordManagerAutoSigninEnabled());

                preferences.finish();

                PrefServiceBridge.getInstance().setPasswordManagerAutoSigninEnabled(false);
            }
        });

        final Preferences preferences2 = PreferencesTest.startPreferences(
                getInstrumentation(), ManageSavedPasswordsPreferences.class.getName());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertTrue(CommandLine.getInstance().hasSwitch(
                        ContentSwitches.ENABLE_CREDENTIAL_MANAGER_API));
                ManageSavedPasswordsPreferences passwordPrefs =
                        (ManageSavedPasswordsPreferences) preferences2.getFragmentForTest();
                ChromeBaseCheckBoxPreference onOffSwitch =
                        (ChromeBaseCheckBoxPreference) passwordPrefs.findPreference(
                                ManageSavedPasswordsPreferences.PREF_AUTOSIGNIN_SWITCH);
                assertFalse(onOffSwitch.isChecked());
            }
        });
    }
}
