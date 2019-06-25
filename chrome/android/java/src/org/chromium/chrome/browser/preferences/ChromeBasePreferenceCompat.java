// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceViewHolder;
import android.util.AttributeSet;
import android.widget.TextView;

import org.chromium.chrome.R;

/**
 * A preference that supports some Chrome-specific customizations:
 *
 * 1. This preference supports being managed. If this preference is managed (as determined by its
 *    ManagedPreferenceDelegate), it updates its appearance and behavior appropriately: shows an
 *    enterprise icon, disables clicks, etc.
 *
 * 2. This preference can have a multiline title.
 * 3. This preference can set an icon color in XML through app:iconTint. Note that if a
 *    ColorStateList is set, only the default color will be used.
 *
 * TODO(crbug.com/967022): This class is analogous to {@link ChromeBasePreference}, but extends the
 * Preference Support Library rather than the deprecated Framework preferences. Once all {@link
 * ChromeBasePreference}s have been migrated to the Support Library, {@link ChromeBasePreference}
 * will be removed in favor of {@link ChromeBasePreferenceCompat}.
 */
public class ChromeBasePreferenceCompat extends Preference {
    private ColorStateList mIconTint;
    private ManagedPreferenceDelegateCompat mManagedPrefDelegate;

    /**
     * Constructor for use in Java.
     */
    public ChromeBasePreferenceCompat(Context context) {
        this(context, null);
    }

    /**
     * Constructor for inflating from XML.
     */
    public ChromeBasePreferenceCompat(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.preference_compat);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.ChromeBasePreference);
        mIconTint = a.getColorStateList(R.styleable.ChromeBasePreference_iconTint);
        a.recycle();
    }

    /**
     * Sets the ManagedPreferenceDelegate which will determine whether this preference is managed.
     */
    public void setManagedPreferenceDelegate(ManagedPreferenceDelegateCompat delegate) {
        mManagedPrefDelegate = delegate;
        ManagedPreferencesUtils.initPreference(mManagedPrefDelegate, this);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        ((TextView) holder.findViewById(android.R.id.title)).setSingleLine(false);
        Drawable icon = getIcon();
        if (icon != null && mIconTint != null) {
            icon.setColorFilter(mIconTint.getDefaultColor(), PorterDuff.Mode.SRC_IN);
        }
        ManagedPreferencesUtils.onBindViewToPreference(mManagedPrefDelegate, this, holder.itemView);
    }

    @Override
    protected void onClick() {
        if (ManagedPreferencesUtils.onClickPreference(mManagedPrefDelegate, this)) return;
        super.onClick();
    }
}
