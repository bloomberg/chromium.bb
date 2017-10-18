// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.support.annotation.NonNull;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.TintedImageView;
import org.chromium.chrome.browser.widget.bottomsheet.base.BottomNavigationItemView;

/**
 * An implementation of the forked {@link BottomNavigationItemView} specifically for the Chrome Home
 * bottom navigation menu.
 *
 * Uses a different layout XML that simplifies the menu item with only one label, and uses a
 * {@link TintedImageView} to simplify the icon tinting logic.
 */
public class BottomSheetNavigationItemView extends BottomNavigationItemView {
    private boolean mLabelHidden;
    private TintedImageView mIcon;
    private TextView mLabel;

    public BottomSheetNavigationItemView(@NonNull Context context) {
        super(context);
    }

    @Override
    protected void initVisuals(Context context) {
        LayoutInflater.from(context).inflate(R.layout.bottom_sheet_nav_menu_item, this, true);
        setOrientation(LinearLayout.VERTICAL);
        setGravity(Gravity.CENTER);
        setBackgroundResource(
                android.support.design.R.drawable.design_bottom_navigation_item_background);
        mIcon = (TintedImageView) findViewById(R.id.icon);
        mLabel = (TextView) findViewById(R.id.label);
    }

    @Override
    public void setTitle(CharSequence title) {
        mLabel.setText(title);
    }

    @Override
    public void setIcon(Drawable icon) {
        mIcon.setImageDrawable(icon);
    }

    @Override
    public void setChecked(boolean checked) {
        refreshDrawableState();
    }

    @Override
    public void setEnabled(boolean enabled) {
        mLabel.setEnabled(enabled);
        mIcon.setEnabled(enabled);
    }

    @Override
    public void hideLabel() {
        if (mLabel == null || mLabelHidden) return;
        mLabelHidden = true;
        mLabel.setVisibility(GONE);
        setContentDescription(mItemData.getTitle());
    }

    @Override
    public void setIconTint(ColorStateList tint) {
        mIcon.setTint(tint);
    }

    @Override
    public void setTextColors(ColorStateList colors) {
        mLabel.setTextColor(colors);
    }
}