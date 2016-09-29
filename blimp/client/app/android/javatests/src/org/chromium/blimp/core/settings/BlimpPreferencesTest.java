// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.settings;

import android.app.FragmentManager;
import android.content.Intent;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.preference.SwitchPreference;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityInstrumentationTestCase;
import org.chromium.blimp.core.MockBlimpClientContext;
import org.chromium.blimp.core.MockBlimpClientContextDelegate;
import org.chromium.blimp.core.common.PreferencesUtil;
import org.chromium.components.signin.ChromeSigninController;

/**
 * Test blimp setting page.
 */
public class BlimpPreferencesTest extends BaseActivityInstrumentationTestCase<MockPreferences> {

    public BlimpPreferencesTest() {
        super(MockPreferences.class);
    }

    // Launch the testing activity from non UI thread.
    private void launchActivity() {
        assertFalse(ThreadUtils.runningOnUiThread());
        Intent intent = new Intent(Intent.ACTION_MAIN);
        setActivityIntent(intent);

        // Start the activity.
        getActivity();
    }

    /**
     *  Mock {@link AboutBlimpPreferences}, the main test target class.
     */
    public static class MockAboutBlimpPreferences extends AboutBlimpPreferences {
        private static final String BLIMP_PREF_TAG = "TestBlimpPref";

        // Avoid calling native code in instrumentation test.
        @Override
        protected void initializeNative() {}
        @Override
        protected void destroyNative() {}

        public void testRestartBrowser() {
            super.restartBrowser();
        }
    }

    private Preference attachBlimpPref(PreferenceFragment fragment) {
        ThreadUtils.assertOnUiThread();
        assertNotNull(fragment);
        MockAboutBlimpPreferences.addBlimpPreferences(fragment, new MockBlimpClientContext());
        return fragment.findPreference(PreferencesUtil.PREF_BLIMP_SWITCH);
    }

    // Create the setting page preference.
    private MockAboutBlimpPreferences createMockBlimpPreferenceFragment() {
        FragmentManager manager = getActivity().getFragmentManager();
        manager.beginTransaction()
                .add(new MockAboutBlimpPreferences(), MockAboutBlimpPreferences.BLIMP_PREF_TAG)
                .commitAllowingStateLoss();
        manager.executePendingTransactions();

        MockAboutBlimpPreferences blimpFragment =
                (MockAboutBlimpPreferences) manager.findFragmentByTag(
                        MockAboutBlimpPreferences.BLIMP_PREF_TAG);

        assertNotNull("Blimp PreferenceFragment is not found.", blimpFragment);

        // Call onResume to load preferences items, this simulate the actual fragment life cycle.
        blimpFragment.onResume();

        return blimpFragment;
    }

    /**
     * Test if blimp settings can be attached correctly, and if callback is handled correctly.
     */
    @SmallTest
    public void testBlimpPref() {
        launchActivity();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PreferenceFragment mainFragment = getActivity().getMainFragment();
                assertNotNull("Mock Main preferences is null.", mainFragment);

                PreferenceScreen mainScreen = mainFragment.getPreferenceScreen();
                assertNotNull("Mock Main preferences screen is null.", mainScreen);

                // Check if blimp settings preference item can be attached to main preferences list.
                Preference blimpPref = attachBlimpPref(mainFragment);
                assertNotNull("Blimp preference item in main preferences list is not found.",
                        blimpPref);

                // Open blimp settings page and check if callback is handled.
                MockAboutBlimpPreferences blimpFragment = createMockBlimpPreferenceFragment();
                MockBlimpClientContext mockClientContext = new MockBlimpClientContext();
                MockAboutBlimpPreferences.setDelegate(mockClientContext);

                blimpFragment.testRestartBrowser();
                MockBlimpClientContextDelegate mockDelegate =
                        (MockBlimpClientContextDelegate) mockClientContext.getDelegate();
                assertEquals("Restart browser should be called.",
                        mockDelegate.restartBrowserCalled(), 1);
            }
        });
    }

    /**
     * Test the Blimp switch preference when user signed in or signed out.
     */
    @SmallTest
    public void testSwitchPrefSignedIn() {
        launchActivity();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Mock user sign in.
                ChromeSigninController signinController =
                        ChromeSigninController.get(ContextUtils.getApplicationContext());
                signinController.setSignedInAccountName("test@gmail.com");
                assertTrue("User should be signed in.", signinController.isSignedIn());

                MockBlimpClientContext mockClientContext = new MockBlimpClientContext();
                MockAboutBlimpPreferences.setDelegate(mockClientContext);

                MockAboutBlimpPreferences blimpFragment = createMockBlimpPreferenceFragment();

                // Test switch preference states after the setting page is created.
                SwitchPreference switchPref = (SwitchPreference) blimpFragment.findPreference(
                        PreferencesUtil.PREF_BLIMP_SWITCH);
                boolean canUpdate = switchPref.getOnPreferenceChangeListener().onPreferenceChange(
                        switchPref, true);
                assertTrue("User can change switch if the user signed in.", canUpdate);
                assertEquals(
                        "Connect should be called.", mockClientContext.connectCalledCount(), 1);
                mockClientContext.reset();

                // Test switch state after user signed out.
                signinController.setSignedInAccountName(null);
                blimpFragment.onResume();

                // Need to update the reference of switchPreference since UI will remove all the
                // prefs, and fill in with new ones in onResume.
                switchPref = (SwitchPreference) blimpFragment.findPreference(
                        PreferencesUtil.PREF_BLIMP_SWITCH);
                canUpdate = switchPref.getOnPreferenceChangeListener().onPreferenceChange(
                        switchPref, true);
                assertFalse("User can't change switch if the user signed out.", canUpdate);

                // Test waiting for sign in and sign in call from native code.
                blimpFragment.mWaitForSignIn = true;
                blimpFragment.onSignedIn();
                switchPref = (SwitchPreference) blimpFragment.findPreference(
                        PreferencesUtil.PREF_BLIMP_SWITCH);
                assertFalse(
                        "UI should no longer wait for user sign in.", blimpFragment.mWaitForSignIn);
                assertTrue("User sign in call should turn on the switch.", switchPref.isChecked());
                assertEquals(
                        "Connect should be called.", mockClientContext.connectCalledCount(), 1);
                mockClientContext.reset();

                // Test sign out call from native code.
                blimpFragment.onSignedOut();
                switchPref = (SwitchPreference) blimpFragment.findPreference(
                        PreferencesUtil.PREF_BLIMP_SWITCH);
                assertFalse(
                        "User sign out call should turn off the switch.", switchPref.isChecked());
            }
        });
    }
}
