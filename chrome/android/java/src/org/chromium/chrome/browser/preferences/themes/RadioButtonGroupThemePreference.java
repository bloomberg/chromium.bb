// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.themes;

import android.content.Context;
import android.preference.Preference;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.themes.ThemePreferences.ThemeSetting;
import org.chromium.chrome.browser.widget.RadioButtonWithDescription;

import java.util.Arrays;
import java.util.List;

/**
 * A radio button group Preference used for Themes. Currently, it has 3 options: System default,
 * Light, and Dark.
 */
public class RadioButtonGroupThemePreference
        extends Preference implements RadioButtonWithDescription.OnCheckedChangeListener {
    private @ThemeSetting int mSetting;

    private RadioButtonWithDescription mSystemDefault;
    private RadioButtonWithDescription mLight;
    private RadioButtonWithDescription mDark;

    public RadioButtonGroupThemePreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // Inflating from XML.
        setLayoutResource(R.layout.radio_button_group_theme_preference);
    }

    /**
     * @param setting The initial setting for this Preference
     */
    public void initialize(@ThemeSetting int setting) {
        mSetting = setting;
    }

    @Override
    protected void onBindView(View view) {
        super.onBindView(view);

        mSystemDefault = view.findViewById(R.id.system_default);
        mLight = view.findViewById(R.id.light);
        mDark = view.findViewById(R.id.dark);

        List<RadioButtonWithDescription> radioGroup = Arrays.asList(mSystemDefault, mLight, mDark);
        for (RadioButtonWithDescription option : radioGroup) {
            option.setRadioButtonGroup(radioGroup);
            option.setOnCheckedChangeListener(this);
        }

        RadioButtonWithDescription radioButton = findRadioButton(mSetting);
        if (radioButton != null) radioButton.setChecked(true);
    }

    @Override
    public void onCheckedChanged() {
        if (mSystemDefault.isChecked()) {
            mSetting = ThemeSetting.SYSTEM_DEFAULT;
        } else if (mLight.isChecked()) {
            mSetting = ThemeSetting.LIGHT;
        } else if (mDark.isChecked()) {
            mSetting = ThemeSetting.DARK;
        }

        callChangeListener(mSetting);
    }

    /**
     * @param setting The setting to find RadioButton for.
     */
    private RadioButtonWithDescription findRadioButton(@ThemeSetting int setting) {
        if (setting == ThemeSetting.SYSTEM_DEFAULT) {
            return mSystemDefault;
        } else if (setting == ThemeSetting.LIGHT) {
            return mLight;
        } else if (setting == ThemeSetting.DARK) {
            return mDark;
        } else {
            return null;
        }
    }
}
