// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.content.res.TypedArray;
import android.preference.SwitchPreference;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.ChromeSwitchCompat;

/**
 * A super-powered SwitchPreference designed especially for Chrome. Special features:
 *  - Supports managed preferences
 *  - Displays a material-styled switch, even on pre-L devices
 */
public class ChromeSwitchPreference extends SwitchPreference {

    private ManagedPreferenceDelegate mManagedPrefDelegate;

    private boolean mDontUseSummaryAsTitle;

    /**
     * Constructor for inflating from XML.
     */
    public ChromeSwitchPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setWidgetLayoutResource(R.layout.preference_switch);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.ChromeSwitchPreference);
        mDontUseSummaryAsTitle =
                a.getBoolean(R.styleable.ChromeSwitchPreference_dontUseSummaryAsTitle, false);
        a.recycle();
    }

    /**
     * Sets the ManagedPreferenceDelegate which will determine whether this preference is managed.
     */
    public void setManagedPreferenceDelegate(ManagedPreferenceDelegate delegate) {
        mManagedPrefDelegate = delegate;
        if (mManagedPrefDelegate != null) mManagedPrefDelegate.initPreference(this);
    }

    @Override
    protected void onBindView(View view) {
        super.onBindView(view);

        ChromeSwitchCompat switchView = (ChromeSwitchCompat) view.findViewById(R.id.switch_widget);
        // On BLU Life Play devices SwitchPreference.setWidgetLayoutResource() does nothing. As a
        // result, the user will see a non-material Switch and switchView will be null, hence the
        // null check below. http://crbug.com/451447
        if (switchView != null) {
            switchView.setChecked(isChecked());
        }

        TextView title = (TextView) view.findViewById(android.R.id.title);
        title.setSingleLine(false);
        if (!mDontUseSummaryAsTitle && TextUtils.isEmpty(getTitle())) {
            TextView summary = (TextView) view.findViewById(android.R.id.summary);
            title.setText(summary.getText());
            title.setVisibility(View.VISIBLE);
            summary.setVisibility(View.GONE);
        }

        if (mManagedPrefDelegate != null) mManagedPrefDelegate.onBindViewToPreference(this, view);
    }

    @Override
    protected void onClick() {
        if (mManagedPrefDelegate != null && mManagedPrefDelegate.onClickPreference(this)) return;
        super.onClick();
    }
}
