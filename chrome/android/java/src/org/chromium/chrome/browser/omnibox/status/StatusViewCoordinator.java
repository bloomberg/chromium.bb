// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.app.Activity;
import android.content.res.Resources;
import android.support.annotation.DrawableRes;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.omnibox.status.StatusView.NavigationButtonType;
import org.chromium.chrome.browser.omnibox.status.StatusView.StatusButtonType;
import org.chromium.chrome.browser.page_info.PageInfoController;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * A component for displaying a status icon (e.g. security icon or navigation icon) and optional
 * verbose status text.
 */
public class StatusViewCoordinator implements View.OnClickListener {
    private final boolean mIsTablet;
    private final StatusView mStatusView;
    private final StatusMediator mMediator;

    private ToolbarDataProvider mToolbarDataProvider;
    private WindowAndroid mWindowAndroid;

    // The type of the security icon currently active.
    @DrawableRes
    private int mSecurityIconResource;

    @StatusButtonType
    private int mStatusButtonType;
    private boolean mUrlHasFocus;

    /**
     * Creates a new StatusViewCoordinator.
     * @param isTablet Whether the UI is shown on a tablet.
     * @param statusView The status view, used to supply and manipulate child views.
     */
    public StatusViewCoordinator(boolean isTablet, StatusView statusView) {
        mIsTablet = isTablet;
        mStatusView = statusView;

        PropertyModel model =
                new PropertyModel.Builder(StatusProperties.ALL_KEYS)
                        .with(StatusProperties.ICON_TINT_COLOR_RES,
                                R.color.locationbar_status_separator_color)
                        .with(StatusProperties.STATUS_BUTTON_TYPE, StatusButtonType.NONE)
                        .build();

        PropertyModelChangeProcessor.create(model, mStatusView, new StatusViewBinder());
        mMediator = new StatusMediator(model);

        Resources res = mStatusView.getResources();
        mMediator.setUrlMinWidth(res.getDimensionPixelSize(R.dimen.location_bar_min_url_width)
                + res.getDimensionPixelSize(R.dimen.location_bar_start_icon_width)
                + (res.getDimensionPixelSize(R.dimen.location_bar_lateral_padding) * 2));

        mMediator.setSeparatorFieldMinWidth(
                res.getDimensionPixelSize(R.dimen.location_bar_status_separator_width)
                + res.getDimensionPixelSize(R.dimen.location_bar_status_separator_spacer));

        mMediator.setVerboseStatusTextMinWidth(
                res.getDimensionPixelSize(R.dimen.location_bar_min_verbose_status_text_width));

        if (mIsTablet) {
            mMediator.setNavigationButtonType(NavigationButtonType.PAGE);
        }

        mSecurityIconResource = 0;
    }

    /**
     * Provides data and state for the toolbar component.
     * @param toolbarDataProvider The data provider.
     */
    public void setToolbarDataProvider(ToolbarDataProvider toolbarDataProvider) {
        mToolbarDataProvider = toolbarDataProvider;
    }

    /**
     * Provides the WindowAndroid for the parent component. Used to retreive the current activity
     * on demand.
     * @param windowAndroid The current {@link WindowAndroid}.
     */
    public void setWindowAndroid(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
    }

    /**
     * Signals that native initialization has completed.
     */
    public void onNativeInitialized() {
        mStatusView.getSecurityButton().setOnClickListener(this);
        mStatusView.getNavigationButton().setOnClickListener(this);
        mStatusView.getVerboseStatusTextView().setOnClickListener(this);
    }

    /**
     * @param urlHasFocus Whether the url currently has focus.
     */
    public void onUrlFocusChange(boolean urlHasFocus) {
        mMediator.setUrlHasFocus(urlHasFocus);
        mUrlHasFocus = urlHasFocus;
        changeLocationBarIcon();
        updateVerboseStatusVisibility();
        updateLocationBarIconContainerVisibility();
    }

    /**
     * @param useDarkColors Whether dark colors should be for the status icon and text.
     */
    public void setUseDarkColors(boolean useDarkColors) {
        mMediator.setUseDarkColors(useDarkColors);

        // TODO(ender): remove this once icon selection has complete set of
        // corresponding properties (for tinting etc).
        updateSecurityIcon();
    }

    @StatusButtonType
    private int getLocationBarButtonToShow() {
        // The navigation icon type is only applicable on tablets.  While smaller form factors do
        // not have an icon visible to the user when the URL is focused, BUTTON_TYPE_NONE is not
        // returned as it will trigger an undesired jump during the animation as it attempts to
        // hide the icon.
        if (mUrlHasFocus && mIsTablet) return StatusButtonType.NAVIGATION_ICON;

        return mToolbarDataProvider.getSecurityIconResource(mIsTablet) != 0
                ? StatusButtonType.SECURITY_ICON
                : StatusButtonType.NONE;
    }

    private void changeLocationBarIcon() {
        mStatusButtonType = getLocationBarButtonToShow();
        mMediator.setStatusButtonType(mStatusButtonType);
    }

    /**
     * Updates the security icon displayed in the LocationBar.
     */
    public void updateSecurityIcon() {
        @DrawableRes
        int id = mToolbarDataProvider.getSecurityIconResource(mIsTablet);
        if (id == 0) {
            mStatusView.getSecurityButton().setImageDrawable(null);
        } else {
            // ImageView#setImageResource is no-op if given resource is the current one.
            mStatusView.getSecurityButton().setImageResource(id);
            ApiCompatibilityUtils.setImageTintList(mStatusView.getSecurityButton(),
                    mToolbarDataProvider.getSecurityIconColorStateList());
        }

        int contentDescriptionId = mToolbarDataProvider.getSecurityIconContentDescription();
        String contentDescription = mStatusView.getContext().getString(contentDescriptionId);
        mStatusView.getSecurityButton().setContentDescription(contentDescription);

        updateVerboseStatusVisibility();

        if (mSecurityIconResource == id && mStatusButtonType == getLocationBarButtonToShow()) {
            return;
        }
        mSecurityIconResource = id;

        changeLocationBarIcon();
        updateLocationBarIconContainerVisibility();
    }

    /**
     * @return The view displaying the security icon.
     */
    public View getSecurityIconView() {
        return mStatusView.getSecurityButton();
    }

    /**
     * @return Whether the security button is currently being displayed.
     */
    @VisibleForTesting
    public boolean isSecurityButtonShown() {
        return mStatusButtonType == StatusButtonType.SECURITY_ICON;
    }

    /**
     * @return The ID of the drawable currently shown in the security icon.
     */
    @VisibleForTesting
    @DrawableRes
    public int getSecurityIconResourceId() {
        return mSecurityIconResource;
    }

    /**
     * Sets the type of the current navigation type and updates the UI to match it.
     * @param buttonType The type of navigation button to be shown.
     */
    public void setNavigationButtonType(@NavigationButtonType int buttonType) {
        ImageView navigationButton = mStatusView.getNavigationButton();
        // TODO(twellington): Return early if the navigation button type and tint hasn't changed.
        if (!mIsTablet) return;

        mMediator.setNavigationButtonType(buttonType);

        if (navigationButton.getVisibility() != View.VISIBLE) {
            navigationButton.setVisibility(View.VISIBLE);
        }

        updateLocationBarIconContainerVisibility();
    }

    /**
     * @param visible Whether the navigation button should be visible.
     */
    public void setSecurityButtonVisibility(boolean visible) {
        ImageButton securityButton = mStatusView.getSecurityButton();
        if (!visible && securityButton.getVisibility() == View.VISIBLE) {
            securityButton.setVisibility(View.GONE);
        } else if (visible && securityButton.getVisibility() == View.GONE
                && securityButton.getDrawable() != null
                && securityButton.getDrawable().getIntrinsicWidth() > 0
                && securityButton.getDrawable().getIntrinsicHeight() > 0) {
            securityButton.setVisibility(View.VISIBLE);
        }
    }

    /**
     * Update visibility of the verbose status based on the button type and focus state of the
     * omnibox.
     */
    private void updateVerboseStatusVisibility() {
        // TODO(ender): turn around logic for ToolbarDataProvider to offer
        // notifications rather than polling for these attributes.
        mMediator.setVerboseStatusTextAllowed(mToolbarDataProvider.shouldShowVerboseStatus());
        mMediator.setPageIsOffline(mToolbarDataProvider.isOfflinePage());
        mMediator.setPageIsPreview(mToolbarDataProvider.isPreview());
    }

    /**
     * Update the visibility of the location bar icon container based on the state of the
     * security and navigation icons.
     */
    protected void updateLocationBarIconContainerVisibility() {
        @StatusButtonType
        int buttonToShow = getLocationBarButtonToShow();
        mStatusView.findViewById(R.id.location_bar_icon)
                .setVisibility(buttonToShow != StatusButtonType.NONE ? View.VISIBLE : View.GONE);
    }

    /**
     * Whether {@code v} is a view (location icon, verbose status, ...) which can be clicked to
     * show the Page Info popup.
     */
    public boolean shouldShowPageInfoForView(View v) {
        return v == mStatusView.getSecurityButton() || v == mStatusView.getNavigationButton()
                || v == mStatusView.getVerboseStatusTextView();
    }

    @Override
    public void onClick(View view) {
        if (mUrlHasFocus || !shouldShowPageInfoForView(view)) return;

        if (!mToolbarDataProvider.hasTab() || mToolbarDataProvider.getTab().getWebContents() == null
                || mWindowAndroid == null) {
            return;
        }

        Activity activity = mWindowAndroid.getActivity().get();
        if (activity != null) {
            PageInfoController.show(activity, mToolbarDataProvider.getTab(), null,
                    PageInfoController.OpenedFromSource.TOOLBAR);
        }
    }

    /**
     * Called to set the width of the location bar when the url bar is not focused.
     * This value is used to determine whether the verbose status text should be visible.
     * @param width The unfocused location bar width.
     */
    public void setUnfocusedLocationBarWidth(int width) {
        mMediator.setUnfocusedLocationBarWidth(width);
    }

    /**
     * Toggle animation of icon changes.
     */
    public void setShouldAnimateIconChanges(boolean shouldAnimate) {
        mMediator.setAnimationsEnabled(shouldAnimate);
    }
}
