// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.app.Fragment;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnFocusChangeListener;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.UrlUtilities;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.widget.ChromeSwitchCompat;
import org.chromium.ui.UiUtils;

/**
 * Fragment that allows the user to configure homepage related preferences.
 */
public class HomepagePreferences extends Fragment {
    private HomepageManager mHomepageManager;
    private ChromeSwitchCompat mHomepageSwitch;
    private TextView mHomepageSwitchLabel;
    private String mCustomUriCache;
    private EditText mCustomUriEditText;
    private CheckBox mPartnerDefaultCheckbox;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mHomepageManager = HomepageManager.getInstance(getActivity());
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        getActivity().setTitle(R.string.options_homepage_title);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        View rootView = inflater.inflate(R.layout.homepage_preferences, null);

        initHomepageSwitch(rootView);
        initCustomUriEditText(rootView);
        initPartnerDefaultCheckbox(rootView);

        updateUIState();

        return rootView;
    }

    private void initHomepageSwitch(View rootView) {
        mHomepageSwitch = (ChromeSwitchCompat) rootView.findViewById(R.id.homepage_switch);
        mHomepageSwitchLabel = (TextView) rootView.findViewById(R.id.homepage_switch_label);

        boolean isHomepageEnabled = mHomepageManager.getPrefHomepageEnabled();
        mHomepageSwitch.setChecked(isHomepageEnabled);
        setHomepageSwitchLabelText(isHomepageEnabled);

        mHomepageSwitch.setOnCheckedChangeListener(new OnCheckedChangeListener(){
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                mHomepageManager.setPrefHomepageEnabled(isChecked);
                setHomepageSwitchLabelText(isChecked);
                updateUIState();
            }
        });
    }

    private void setHomepageSwitchLabelText(boolean isChecked) {
        if (isChecked) {
            mHomepageSwitchLabel.setText(mHomepageSwitch.getTextOn());
        } else {
            mHomepageSwitchLabel.setText(mHomepageSwitch.getTextOff());
        }
    }

    private void initCustomUriEditText(View rootView) {
        mCustomUriCache = mHomepageManager.getPrefHomepageCustomUri();

        mCustomUriEditText = (EditText) rootView.findViewById(R.id.custom_uri);
        mCustomUriEditText.addTextChangedListener(new TextWatcher() {
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
            }

            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            }

            @Override
            public void afterTextChanged(Editable s) {
                if (mCustomUriEditText.isEnabled()) {
                    mCustomUriCache = s.toString();
                }
            }
        });
        mCustomUriEditText.setOnFocusChangeListener(new OnFocusChangeListener() {
            @Override
            public void onFocusChange(View view, boolean hasFocus) {
                if (!hasFocus) UiUtils.hideKeyboard(view);
            }
        });
    }

    private void initPartnerDefaultCheckbox(View rootView) {
        mPartnerDefaultCheckbox = (CheckBox) rootView.findViewById(R.id.default_checkbox);
        mPartnerDefaultCheckbox.setOnCheckedChangeListener(new OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                mHomepageManager.setPrefHomepageUseDefaultUri(isChecked);
                updateUIState();
            }
        });
    }

    private void updateUIState() {
        boolean isHomepageEnabled = mHomepageManager.getPrefHomepageEnabled();
        boolean isHomepagePartnerEnabled =
                mHomepageManager.getPrefHomepageUseDefaultUri();

        mCustomUriEditText.setEnabled(false);
        mCustomUriEditText.setText(isHomepagePartnerEnabled
                ? PartnerBrowserCustomizations.getHomePageUrl() : mCustomUriCache);
        mCustomUriEditText.setEnabled(isHomepageEnabled && !isHomepagePartnerEnabled);

        mPartnerDefaultCheckbox.setChecked(isHomepagePartnerEnabled);
        mPartnerDefaultCheckbox.setEnabled(isHomepageEnabled);
    }

    @Override
    public void onStop() {
        super.onStop();
        mHomepageManager.setPrefHomepageCustomUri(
                UrlUtilities.fixUrl(UrlUtilities.fixupUrl(mCustomUriCache)));
    }

}
