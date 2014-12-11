// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.preference.SwitchPreference;
import android.support.v7.widget.SwitchCompat;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;

/**
 * A super-powered SwitchPreference designed especially for Chrome. Special features:
 *  - Supports managed preferences
 *  - Displays a material-styled switch, even on pre-L devices
 */
public class ChromeSwitchPreference extends SwitchPreference {

    private ManagedPreferenceDelegate mManagedPrefDelegate;

    /**
     * Constructor for inflating from XML.
     */
    public ChromeSwitchPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setWidgetLayoutResource(R.layout.preference_switch);
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
        SwitchCompat switchView = (SwitchCompat) view.findViewById(R.id.switch_widget);
        switchView.setChecked(isChecked());
        TextView title = (TextView) view.findViewById(android.R.id.title);
        title.setSingleLine(false);

        // If the title is empty, make the summary text look like the switch title
        if (TextUtils.isEmpty(getTitle())) {
            // On some devices/Android versions, the title view doesn't get properly hidden when
            // it's empty, resulting in the summary text being misaligned. See crbug.com/436685
            title.setVisibility(View.GONE);

            TextView summary = (TextView) view.findViewById(android.R.id.summary);
            summary.setTextSize(TypedValue.COMPLEX_UNIT_PX, title.getTextSize());
            summary.setTextColor(title.getTextColors());
        }

        if (mManagedPrefDelegate != null) mManagedPrefDelegate.onBindViewToPreference(this, view);
    }

    @Override
    protected void onClick() {
        if (mManagedPrefDelegate != null && mManagedPrefDelegate.onClickPreference(this)) return;
        super.onClick();
    }
}
