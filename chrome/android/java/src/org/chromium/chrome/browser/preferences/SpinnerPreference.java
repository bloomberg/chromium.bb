// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.preference.Preference;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.chrome.R;

/**
 * A preference that takes value from a specified list of objects, presented as a dropdown.
 */
public class SpinnerPreference extends Preference {
    private Spinner mSpinner;
    private ArrayAdapter<Object> mAdapter;
    private int mSelectedIndex;

    /**
     * Constructor for inflating from XML.
     */
    public SpinnerPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.preference_spinner);
    }

    /**
     * Provides a list of arbitrary objects to be shown in the spinner. Visually, each option will
     * be presented as its toString() text.
     * @param options The options to be shown in the spinner.
     * @param selectedIndex Index of the initially selected option.
     */
    public void setOptions(Object[] options, int selectedIndex) {
        mAdapter = new ArrayAdapter<Object>(
                getContext(), android.R.layout.simple_spinner_item, options);
        mAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mSelectedIndex = selectedIndex;
    }

    /**
     * @return The currently selected option.
     */
    public Object getSelectedOption() {
        if (mSpinner == null) return null;
        return mSpinner.getSelectedItem();
    }

    @Override
    public View onCreateView(ViewGroup parent) {
        View view = super.onCreateView(parent);

        ((TextView) view.findViewById(R.id.title)).setText(getTitle());
        mSpinner = (Spinner) view.findViewById(R.id.spinner);
        mSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(
                    AdapterView<?> parent, View view, int position, long id) {
                mSelectedIndex = position;
                if (getOnPreferenceChangeListener() != null) {
                    getOnPreferenceChangeListener().onPreferenceChange(
                            SpinnerPreference.this, getSelectedOption());
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
                // No callback. Only update listeners when an actual option is selected.
            }
        });

        return view;
    }

    @Override
    protected void onBindView(View view) {
        mSpinner.setAdapter(mAdapter);
        mSpinner.setSelection(mSelectedIndex);
    }
}
