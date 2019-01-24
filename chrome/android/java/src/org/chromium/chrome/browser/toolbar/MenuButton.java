// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.annotation.DrawableRes;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper.MenuButtonState;

/**
 * The overflow menu button.
 */
public class MenuButton extends FrameLayout implements TintObserver {
    /** The {@link ImageButton} for the menu button. */
    private ImageButton mMenuImageButton;

    /** The view for the update badge. */
    private ImageView mUpdateBadgeView;
    private boolean mUseLightDrawables;

    /** A provider that notifies components when the theme color changes.*/
    private ThemeColorProvider mThemeColorProvider;

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
    public void setTouchListener(OnTouchListener onTouchListener) {
        mMenuImageButton.setOnTouchListener(onTouchListener);
    }

    @Override
    public void setAccessibilityDelegate(AccessibilityDelegate delegate) {
        mMenuImageButton.setAccessibilityDelegate(delegate);
    }

    /**
     * Sets the update badge to visible if the update state requires it.
     *
     * Also updates the visuals to the correct type for the case where {@link
     * #setUseLightDrawables(boolean)} was invoked before the update state was known. This is the
     * case on startup when the bottom toolbar is in use.
     *
     * @param visible Whether the update badge should be visible. Always sets visibility to GONE
     *                if the update type does not require a badge.
     * TODO(crbug.com/865801): Clean this up when MenuButton and UpdateMenuItemHelper is MVCed.
     */
    public void setUpdateBadgeVisibilityIfValidState(boolean visible) {
        MenuButtonState buttonState = UpdateMenuItemHelper.getInstance().getUiState().buttonState;

        visible &= buttonState != null;

        mUpdateBadgeView.setVisibility(visible ? View.VISIBLE : View.GONE);
        if (visible) updateImageResources();
        updateContentDescription(visible);
    }

    /**
     * Sets the visual type of update badge to use (if any).
     * @param useLightDrawables Whether the light drawable should be used.
     */
    public void setUseLightDrawables(boolean useLightDrawables) {
        mUseLightDrawables = useLightDrawables;
        updateImageResources();
    }

    public void updateImageResources() {
        MenuButtonState buttonState = UpdateMenuItemHelper.getInstance().getUiState().buttonState;
        if (buttonState == null) return;
        @DrawableRes
        int drawable = mUseLightDrawables ? buttonState.lightBadgeIcon : buttonState.darkBadgeIcon;
        mUpdateBadgeView.setImageDrawable(
                ApiCompatibilityUtils.getDrawable(getResources(), drawable));
    }

    /**
     * @return Whether the update badge is showing.
     */
    public boolean isShowingAppMenuUpdateBadge() {
        return mUpdateBadgeView.getVisibility() == View.VISIBLE;
    }

    /**
     * Sets the content description for the menu button.
     * @param isUpdateBadgeVisible Whether the update menu badge is visible.
     */
    public void updateContentDescription(boolean isUpdateBadgeVisible) {
        if (isUpdateBadgeVisible) {
            MenuButtonState buttonState =
                    UpdateMenuItemHelper.getInstance().getUiState().buttonState;
            assert buttonState != null : "No button state when trying to show the badge.";
            mMenuImageButton.setContentDescription(
                    getResources().getString(buttonState.menuContentDescription));
        } else {
            mMenuImageButton.setContentDescription(
                    getResources().getString(R.string.accessibility_toolbar_btn_menu));
        }
    }

    public ImageButton getImageButton() {
        return mMenuImageButton;
    }

    public void setThemeColorProvider(ThemeColorProvider themeColorProvider) {
        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addTintObserver(this);
    }

    @Override
    public void onTintChanged(ColorStateList tintList, boolean useLight) {
        ApiCompatibilityUtils.setImageTintList(mMenuImageButton, tintList);
        setUseLightDrawables(useLight);
    }

    public void destroy() {
        if (mThemeColorProvider != null) {
            mThemeColorProvider.removeTintObserver(this);
            mThemeColorProvider = null;
        }
    }
}
