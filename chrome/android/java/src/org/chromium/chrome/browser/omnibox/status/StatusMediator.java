// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.support.annotation.ColorRes;
import android.support.annotation.StringRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.omnibox.status.StatusView.NavigationButtonType;
import org.chromium.chrome.browser.omnibox.status.StatusView.StatusButtonType;

/**
 * Contains the controller logic of the Status component.
 */
class StatusMediator {
    private final PropertyModel mModel;
    private boolean mDarkTheme;
    private boolean mUrlHasFocus;
    private boolean mVerboseStatusAllowed;
    private boolean mVerboseStatusSpaceAvailable;
    private boolean mPageIsPreview;
    private boolean mPageIsOffline;

    private int mUrlMinWidth;
    private int mSeparatorMinWidth;
    private int mVerboseStatusTextMinWidth;

    public StatusMediator(PropertyModel model) {
        mModel = model;
    }

    /**
     * Specify type of displayed location bar button.
     */
    public void setLocationBarButtonType(@StatusButtonType int type) {
        mModel.set(StatusProperties.LOCATION_BAR_BUTTON_TYPE, type);
    }

    /**
     * Specify navigation button image type.
     */
    public void setNavigationButtonType(@NavigationButtonType int buttonType) {
        mModel.set(StatusProperties.NAVIGATION_BUTTON_TYPE, buttonType);
    }

    /**
     * Specify whether displayed page is an offline page.
     */
    public void setPageIsOffline(boolean pageIsOffline) {
        if (mPageIsOffline != pageIsOffline) {
            mPageIsOffline = pageIsOffline;
            updateStatusVisibility();
            updateColorTheme();
        }
    }

    /**
     * Specify whether displayed page is a preview page.
     */
    public void setPageIsPreview(boolean pageIsPreview) {
        if (mPageIsPreview != pageIsPreview) {
            mPageIsPreview = pageIsPreview;
            updateStatusVisibility();
            updateColorTheme();
        }
    }

    /**
     * Toggle animations of icon changes.
     */
    public void setAnimationsEnabled(boolean enabled) {
        mModel.set(StatusProperties.ANIMATIONS_ENABLED, enabled);
    }

    /**
     * Specify minimum width of the separator field.
     */
    public void setSeparatorFieldMinWidth(int width) {
        mSeparatorMinWidth = width;
    }

    /**
     * Update unfocused location bar width to determine shape and content of the
     * Status view.
     */
    public void setUnfocusedLocationBarWidth(int width) {
        // This unfocused width is used rather than observing #onMeasure() to avoid showing the
        // verbose status when the animation to unfocus the URL bar has finished. There is a call to
        // LocationBarLayout#onMeasure() after the URL focus animation has finished and before the
        // location bar has received its updated width layout param.
        int computedSpace = width - mUrlMinWidth - mSeparatorMinWidth;
        boolean hasSpaceForStatus = width >= mVerboseStatusTextMinWidth;

        if (hasSpaceForStatus) {
            mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_WIDTH, computedSpace);
        }

        if (hasSpaceForStatus != mVerboseStatusSpaceAvailable) {
            mVerboseStatusSpaceAvailable = hasSpaceForStatus;
            updateStatusVisibility();
        }
    }

    /**
     * Report URL focus change.
     */
    public void setUrlHasFocus(boolean urlHasFocus) {
        mUrlHasFocus = urlHasFocus;
        updateStatusVisibility();
    }

    /**
     * Specify minimum width of an URL field.
     */
    public void setUrlMinWidth(int width) {
        mUrlMinWidth = width;
    }

    /**
     * Toggle between dark and light UI color theme.
     */
    public void setUseDarkColors(boolean useDarkColors) {
        if (mDarkTheme != useDarkColors) {
            mDarkTheme = useDarkColors;
            updateColorTheme();
        }
    }

    /**
     * Specify whether parent allows verbose status text.
     */
    public void setVerboseStatusTextAllowed(boolean isVerboseStatusTextAllowed) {
        mVerboseStatusAllowed = isVerboseStatusTextAllowed;
        updateStatusVisibility();
    }

    /**
     * Specify minimum width of the verbose status text field.
     */
    public void setVerboseStatusTextMinWidth(int width) {
        mVerboseStatusTextMinWidth = width;
    }

    /**
     * Update visibility of the verbose status text field.
     */
    private void updateStatusVisibility() {
        @StringRes
        int statusText = 0;

        if (mPageIsPreview) {
            statusText = R.string.location_bar_preview_lite_page_status;
        } else if (mPageIsOffline) {
            statusText = R.string.location_bar_verbose_status_offline;
        }

        // Decide whether presenting verbose status text makes sense.
        boolean newVisibility = mVerboseStatusAllowed && mVerboseStatusSpaceAvailable
                && (!mUrlHasFocus) && (statusText != 0);

        // Update status content only if it is visible.
        // Note: PropertyModel will help us avoid duplicate updates with the
        // same value.
        if (newVisibility) {
            mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES, statusText);
        }

        mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE, newVisibility);
    }

    /**
     * Update color theme for all status components.
     */
    private void updateColorTheme() {
        @ColorRes
        int separatorColor = mDarkTheme ? R.color.locationbar_status_separator_color
                                        : R.color.locationbar_status_separator_color_light;

        @ColorRes
        int textColor = 0;
        if (mPageIsPreview) {
            // There will never be a Preview in Incognito and the site theme color is not used. So
            // ignore useDarkColors.
            textColor = R.color.locationbar_status_preview_color;
        } else if (mPageIsOffline) {
            textColor = mDarkTheme ? R.color.locationbar_status_offline_color
                                   : R.color.locationbar_status_offline_color_light;
        }

        @ColorRes
        int tintColor = mDarkTheme ? R.color.dark_mode_tint : R.color.light_mode_tint;

        mModel.set(StatusProperties.ICON_TINT_COLOR_RES, tintColor);
        mModel.set(StatusProperties.SEPARATOR_COLOR_RES, separatorColor);

        if (textColor != 0) mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_COLOR_RES, textColor);
    }
}
