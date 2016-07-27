// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.settings;

import android.app.FragmentManager;
import android.content.Intent;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityInstrumentationTestCase;
import org.chromium.blimp_public.BlimpSettingsCallbacks;

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

    // Mock callback implementation.
    static class MockSettingCallback implements BlimpSettingsCallbacks {
        private int mRestartCalled = 0;
        @Override
        public void onRestartBrowserRequested() {
            ++mRestartCalled;
        }
    }

    // Mock {@link AboutBlimpPreferences}, the main test target class.
    static class MockAboutBlimpPreferences extends AboutBlimpPreferences {
        private static final String BLIMP_PREF_TAG = "TestBlimpPref";
        public void testRestartBrowser() {
            super.restartBrowser();
        }
    }

    private Preference attachBlimpPref(PreferenceFragment fragment,
            BlimpSettingsCallbacks callback) {
        ThreadUtils.assertOnUiThread();
        assertNotNull(fragment);
        MockAboutBlimpPreferences.addBlimpPreferences(fragment);
        MockAboutBlimpPreferences.registerCallback(callback);
        return fragment.findPreference(MockAboutBlimpPreferences.PREF_BLIMP_SWITCH);
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

                MockSettingCallback callback = new MockSettingCallback();

                // Check if blimp settings preference item can be attached to main preferences list.
                Preference blimpPref = attachBlimpPref(mainFragment, callback);
                assertNotNull("Blimp preference item in main preferences list is not found.",
                        blimpPref);

                // Open blimp settings page and check if callback is handled.
                FragmentManager manager = mainFragment.getFragmentManager();
                manager.beginTransaction().add(
                        new MockAboutBlimpPreferences(), MockAboutBlimpPreferences.BLIMP_PREF_TAG)
                            .commitAllowingStateLoss();
                manager.executePendingTransactions();

                MockAboutBlimpPreferences blimpFragment = (MockAboutBlimpPreferences)
                        manager.findFragmentByTag(MockAboutBlimpPreferences.BLIMP_PREF_TAG);
                assertNotNull("Blimp PreferenceFragment is not found.", blimpFragment);

                blimpFragment.testRestartBrowser();
                assertEquals("Unexpected number of callback triggered.", 1,
                        callback.mRestartCalled);
            }
        });
    }
}
