// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.app.settings;

import android.app.Fragment;
import android.content.Intent;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.support.v7.app.AppCompatActivity;

/**
 * An activity to display the settings and version info.
 */
public class Preferences
        extends AppCompatActivity implements PreferenceFragment.OnPreferenceStartFragmentCallback {
    private static final String EXTRA_SHOW_FRAGMENT = "show_fragment";
    private static final String EXTRA_SHOW_FRAGMENT_ARGUMENTS = "show_fragment_args";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String initialFragment = getIntent().getStringExtra(EXTRA_SHOW_FRAGMENT);
        Bundle initialArguments = getIntent().getBundleExtra(EXTRA_SHOW_FRAGMENT_ARGUMENTS);

        if (initialFragment == null) initialFragment = AppBlimpPreferenceScreen.class.getName();
        Fragment fragment = Fragment.instantiate(this, initialFragment, initialArguments);

        getFragmentManager().beginTransaction().replace(android.R.id.content, fragment).commit();
    }

    @Override
    public boolean onPreferenceStartFragment(
            PreferenceFragment preferenceFragment, Preference preference) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setClass(this, getClass());
        intent.putExtra(EXTRA_SHOW_FRAGMENT, preference.getFragment());
        intent.putExtra(EXTRA_SHOW_FRAGMENT_ARGUMENTS, preference.getExtras());
        startActivity(intent);
        return true;
    }
}
