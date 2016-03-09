// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.RadioButton;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.chrome.R;

import java.util.List;

/**
 * A RadioButton with a title and descriptive text to the right.
 */
public class RadioButtonWithDescription extends RelativeLayout implements OnClickListener {
    private RadioButton mRadioButton;
    private TextView mTitle;
    private TextView mDescription;

    private List<RadioButtonWithDescription> mGroup;

    /**
     * Constructor for inflating via XML.
     */
    public RadioButtonWithDescription(Context context, AttributeSet attrs) {
        super(context, attrs);
        LayoutInflater.from(context).inflate(R.layout.radio_button_with_description, this, true);

        mRadioButton = (RadioButton) findViewById(R.id.radio_button);
        mTitle = (TextView) findViewById(R.id.title);
        mDescription = (TextView) findViewById(R.id.description);

        if (attrs != null) applyAttributes(attrs);

        // We want RadioButtonWithDescription to handle the clicks itself.
        mRadioButton.setClickable(false);
        setOnClickListener(this);
    }

    private void applyAttributes(AttributeSet attrs) {
        TypedArray a = getContext().getTheme().obtainStyledAttributes(attrs,
                R.styleable.RadioButtonWithDescription, 0, 0);

        String titleText = a.getString(R.styleable.RadioButtonWithDescription_titleText);
        if (titleText != null) mTitle.setText(titleText);

        a.recycle();
    }

    @Override
    public void onClick(View v) {
        if (mGroup != null) {
            for (RadioButtonWithDescription button : mGroup) {
                button.setChecked(false);
            }
        }

        setChecked(true);
    }

    /**
     * Sets the text shown in the title section.
     */
    public void setTitleText(CharSequence text) {
        mTitle.setText(text);
    }

    /**
     * Sets the text shown in the description section.
     */
    public void setDescriptionText(CharSequence text) {
        mDescription.setText(text);
    }

    /**
     * Returns true if checked.
     */
    public boolean isChecked() {
        return mRadioButton.isChecked();
    }

    /**
     * Sets the checked status.
     */
    public void setChecked(boolean checked) {
        mRadioButton.setChecked(checked);
    }

    /**
     * Sets the group of RadioButtonWithDescriptions that should be unchecked when this button is
     * checked.
     * @param group A list containing all elements of the group.
     */
    public void setRadioButtonGroup(List<RadioButtonWithDescription> group) {
        mGroup = group;
    }
}

