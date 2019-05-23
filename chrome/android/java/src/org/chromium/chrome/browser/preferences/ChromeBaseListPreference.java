// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.preference.ListPreference;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

/**
 * Contains the basic functionality that should be shared by all ListPreference in Chrome.
 */
public class ChromeBaseListPreference extends ListPreference {

    private ManagedPreferenceDelegate mManagedPrefDelegate;

    /**
     * Constructor for inflating from XML.
     */
    public ChromeBaseListPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Sets the ManagedPreferenceDelegate which will determine whether this preference is managed.
     */
    public void setManagedPreferenceDelegate(ManagedPreferenceDelegate delegate) {
        mManagedPrefDelegate = delegate;
        ManagedPreferencesUtils.initPreference(mManagedPrefDelegate, this);
    }

    @Override
    protected void onBindView(View view) {
        super.onBindView(view);
        ((TextView) view.findViewById(android.R.id.title)).setSingleLine(false);
        ManagedPreferencesUtils.onBindViewToPreference(mManagedPrefDelegate, this, view);
    }

    @Override
    protected void onClick() {
        if (ManagedPreferencesUtils.onClickPreference(mManagedPrefDelegate, this)) return;
        super.onClick();
    }
}
