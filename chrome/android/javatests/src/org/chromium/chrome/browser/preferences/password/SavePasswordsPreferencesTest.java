// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.preferences.ChromeBaseCheckBoxPreference;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesTest;
import org.chromium.content.browser.test.NativeLibraryTestRule;

/**
 * Tests for the "Save Passwords" settings screen.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class SavePasswordsPreferencesTest {
    @Rule
    public NativeLibraryTestRule mActivityTestRule = new NativeLibraryTestRule();

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.loadNativeLibraryAndInitBrowserProcess();
    }

    /**
     * Ensure that the on/off switch in "Save Passwords" settings actually enables and disables
     * password saving.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSavePasswordsSwitch() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge.getInstance().setRememberPasswordsEnabled(true);
            }
        });

        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SavePasswordsPreferences savedPasswordPrefs =
                        (SavePasswordsPreferences) preferences.getFragmentForTest();
                ChromeSwitchPreference onOffSwitch = (ChromeSwitchPreference)
                        savedPasswordPrefs.findPreference(
                                SavePasswordsPreferences.PREF_SAVE_PASSWORDS_SWITCH);
                Assert.assertTrue(onOffSwitch.isChecked());

                PreferencesTest.clickPreference(savedPasswordPrefs, onOffSwitch);
                Assert.assertFalse(PrefServiceBridge.getInstance().isRememberPasswordsEnabled());
                PreferencesTest.clickPreference(savedPasswordPrefs, onOffSwitch);
                Assert.assertTrue(PrefServiceBridge.getInstance().isRememberPasswordsEnabled());

                preferences.finish();

                PrefServiceBridge.getInstance().setRememberPasswordsEnabled(false);
            }
        });

        final Preferences preferences2 =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SavePasswordsPreferences savedPasswordPrefs =
                        (SavePasswordsPreferences) preferences2.getFragmentForTest();
                ChromeSwitchPreference onOffSwitch = (ChromeSwitchPreference)
                        savedPasswordPrefs.findPreference(
                                SavePasswordsPreferences.PREF_SAVE_PASSWORDS_SWITCH);
                Assert.assertFalse(onOffSwitch.isChecked());
            }
        });
    }

    /**
     * Ensure that the "Auto Sign-in" switch in "Save Passwords" settings actually enables and
     * disables auto sign-in.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAutoSignInCheckbox() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge.getInstance().setPasswordManagerAutoSigninEnabled(true);
            }
        });

        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SavePasswordsPreferences passwordPrefs =
                        (SavePasswordsPreferences) preferences.getFragmentForTest();
                ChromeBaseCheckBoxPreference onOffSwitch =
                        (ChromeBaseCheckBoxPreference) passwordPrefs.findPreference(
                                SavePasswordsPreferences.PREF_AUTOSIGNIN_SWITCH);
                Assert.assertTrue(onOffSwitch.isChecked());

                PreferencesTest.clickPreference(passwordPrefs, onOffSwitch);
                Assert.assertFalse(
                        PrefServiceBridge.getInstance().isPasswordManagerAutoSigninEnabled());
                PreferencesTest.clickPreference(passwordPrefs, onOffSwitch);
                Assert.assertTrue(
                        PrefServiceBridge.getInstance().isPasswordManagerAutoSigninEnabled());

                preferences.finish();

                PrefServiceBridge.getInstance().setPasswordManagerAutoSigninEnabled(false);
            }
        });

        final Preferences preferences2 =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SavePasswordsPreferences passwordPrefs =
                        (SavePasswordsPreferences) preferences2.getFragmentForTest();
                ChromeBaseCheckBoxPreference onOffSwitch =
                        (ChromeBaseCheckBoxPreference) passwordPrefs.findPreference(
                                SavePasswordsPreferences.PREF_AUTOSIGNIN_SWITCH);
                Assert.assertFalse(onOffSwitch.isChecked());
            }
        });
    }
}
