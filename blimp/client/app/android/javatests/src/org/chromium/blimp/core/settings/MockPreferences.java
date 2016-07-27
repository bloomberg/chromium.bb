// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.settings;

import android.app.Activity;
import android.app.FragmentManager;
import android.os.Bundle;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;

/**
 * Activity to hold {@link MockMainPreferences}.
 */
public class MockPreferences extends Activity {

    public static final String FRAGMENT_TAG = "TestMainPref";

    // Mock {@link MainPreferences} in Clank, this is a PreferenceFragment that holds a list of
    // sub preferences items, including AboutBlimpPreferences.
    static class MockMainPreferences extends PreferenceFragment {
        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            // Create a PreferenceScreen in runtime, since the mock class don't load any XML
            // resource.
            PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getActivity());
            setPreferenceScreen(screen);
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Attach the {@link MockMainPreferences} to this activity.
        FragmentManager manager = getFragmentManager();
        manager.beginTransaction().add(new MockMainPreferences(), FRAGMENT_TAG)
            .commitAllowingStateLoss();
        manager.executePendingTransactions();
    }

    public PreferenceFragment getMainFragment() {
        return (PreferenceFragment) getFragmentManager().findFragmentByTag(FRAGMENT_TAG);
    }
}
