// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.v7.widget.AppCompatImageButton;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;

/**
 * The overflow menu button.
 */
class MenuButton extends FrameLayout {
    /** The {@link android.support.v7.widget.AppCompatImageButton} for the menu button. */
    private AppCompatImageButton mMenuImageButton;

    /** The view for the update badge. */
    private View mUpdateBadgeView;

    public MenuButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMenuImageButton = findViewById(R.id.menu_button);
        mUpdateBadgeView = findViewById(R.id.menu_badge);
    }

    /**
     * @param onTouchListener An {@link OnTouchListener} that is triggered when the menu button is
     *                        clicked.
     */
    void setTouchListener(OnTouchListener onTouchListener) {
        mMenuImageButton.setOnTouchListener(onTouchListener);
    }

    @Override
    public void setAccessibilityDelegate(AccessibilityDelegate delegate) {
        mMenuImageButton.setAccessibilityDelegate(delegate);
    }

    /**
     * @param visible Whether the update badge should be visible.
     */
    void setUpdateBadgeVisibility(boolean visible) {
        mUpdateBadgeView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /**
     * @return Whether the update badge is showing.
     */
    boolean isShowingAppMenuUpdateBadge() {
        return mUpdateBadgeView.getVisibility() == View.VISIBLE;
    }

    View getMenuButton() {
        return mMenuImageButton;
    }

    /**
     * @param tintList The {@link ColorStateList} that will tint the menu button (the badge is not
     *                 tinted).
     */
    void setTint(ColorStateList tintList) {
        ApiCompatibilityUtils.setImageTintList(mMenuImageButton, tintList);
    }
}
