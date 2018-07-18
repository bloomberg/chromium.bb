// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.RadioButtonWithDescription;

/**
 * This fragment implements consent bump screen. This screen lets users to enable or customize
 * personalized and non-personalized services.
 */
public class ConsentBumpFragment extends Fragment {
    private static final String TAG = "ConsentBumpFragment";

    public ConsentBumpFragment() {}

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.consent_bump_view, container, false);

        RadioButtonWithDescription noChanges = view.findViewById(R.id.consent_bump_no_changes);
        noChanges.setDescriptionText(getText(R.string.consent_bump_no_changes_description));
        RadioButtonWithDescription turnOn = view.findViewById(R.id.consent_bump_turn_on);
        turnOn.setDescriptionText(getText(R.string.consent_bump_turn_on_description));
        return view;
    }
}
