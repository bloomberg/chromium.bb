// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.app.settings;

import android.os.Bundle;
import android.preference.PreferenceFragment;

import org.chromium.blimp.app.BlimpEnvironment;
import org.chromium.blimp.app.R;
import org.chromium.blimp_public.BlimpClientContext;

/**
 * Fragment to display blimp client and engine version related info.
 */
public class AppBlimpPreferenceScreen extends PreferenceFragment {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getActivity().setTitle(R.string.about_blimp_preferences);

        setPreferenceScreen(getPreferenceManager().createPreferenceScreen(getActivity()));

        BlimpClientContext blimpClientContext =
                BlimpEnvironment.getInstance().getBlimpClientContext();
        blimpClientContext.attachBlimpPreferences(this);
    }
}
