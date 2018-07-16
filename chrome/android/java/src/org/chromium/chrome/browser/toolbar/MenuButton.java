// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.View.OnTouchListener;
import android.widget.FrameLayout;

import org.chromium.chrome.R;

/**
 * The overflow menu button.
 */
class MenuButton extends FrameLayout {
    /** The view for the menu button. */
    private View mMenuButtonView;

    /** The view for the update badge. */
    private View mUpdateBadgeView;

    public MenuButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMenuButtonView = findViewById(R.id.menu_button);
        mUpdateBadgeView = findViewById(R.id.menu_badge);
    }

    /**
     * @param onTouchListener An {@link OnTouchListener} that is triggered when the menu button is
     *                        clicked.
     */
    void setTouchListener(OnTouchListener onTouchListener) {
        mMenuButtonView.setOnTouchListener(onTouchListener);
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
        return mMenuButtonView;
    }
}
