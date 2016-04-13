// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.settings;

import android.app.Activity;
import android.os.Bundle;

/**
 * An activity to display the settings and version info.
 */
public class Preferences extends Activity {
    private static final String TAG = "Preferences";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getFragmentManager()
                .beginTransaction()
                .replace(android.R.id.content, new AboutBlimpPreferences())
                .commit();
    }
}
