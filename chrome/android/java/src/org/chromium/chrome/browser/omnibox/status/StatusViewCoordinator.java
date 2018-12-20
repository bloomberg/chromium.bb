// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.app.Activity;
import android.content.res.Resources;
import android.support.annotation.DrawableRes;
import android.view.View;
import android.widget.ImageView;

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
    private final PropertyModel mModel;

    private ToolbarDataProvider mToolbarDataProvider;
    private WindowAndroid mWindowAndroid;

    private boolean mUrlHasFocus;

    /**
     * Creates a new StatusViewCoordinator.
     * @param isTablet Whether the UI is shown on a tablet.
     * @param statusView The status view, used to supply and manipulate child views.
     */
    public StatusViewCoordinator(boolean isTablet, StatusView statusView) {
        mIsTablet = isTablet;
        mStatusView = statusView;

        mModel = new PropertyModel.Builder(StatusProperties.ALL_KEYS)
                         .with(StatusProperties.ICON_TINT_COLOR_RES,
                                 R.color.locationbar_status_separator_color)
                         .with(StatusProperties.STATUS_BUTTON_TYPE, StatusButtonType.NONE)
                         .build();

        PropertyModelChangeProcessor.create(mModel, mStatusView, new StatusViewBinder());
        mMediator = new StatusMediator(mModel);

        Resources res = mStatusView.getResources();
        mMediator.setUrlMinWidth(res.getDimensionPixelSize(R.dimen.location_bar_min_url_width)
                + res.getDimensionPixelSize(R.dimen.location_bar_start_icon_width)
                + (res.getDimensionPixelSize(R.dimen.location_bar_lateral_padding) * 2));

        mMediator.setSeparatorFieldMinWidth(
                res.getDimensionPixelSize(R.dimen.location_bar_status_separator_width)
                + res.getDimensionPixelSize(R.dimen.location_bar_status_separator_spacer));

        mMediator.setVerboseStatusTextMinWidth(
                res.getDimensionPixelSize(R.dimen.location_bar_min_verbose_status_text_width));

        mMediator.setTabletMode(mIsTablet);
        mMediator.setNavigationButtonType(NavigationButtonType.PAGE);
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
        updateVerboseStatusVisibility();
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

    /**
     * Updates the security icon displayed in the LocationBar.
     */
    public void updateSecurityIcon() {
        mMediator.setSecurityIconResource(mToolbarDataProvider.getSecurityIconResource(mIsTablet));
        mMediator.setSecurityIconTint(mToolbarDataProvider.getSecurityIconColorStateList());
        mMediator.setSecurityIconDescription(
                mToolbarDataProvider.getSecurityIconContentDescription());

        // TODO(ender): drop these during final cleanup round.
        updateVerboseStatusVisibility();
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
        return mModel.get(StatusProperties.STATUS_BUTTON_TYPE) == StatusButtonType.SECURITY_ICON;
    }

    /**
     * @return The ID of the drawable currently shown in the security icon.
     */
    @VisibleForTesting
    @DrawableRes
    public int getSecurityIconResourceId() {
        return mModel.get(StatusProperties.SECURITY_ICON_RES);
    }

    /**
     * Sets the type of the current navigation type and updates the UI to match it.
     * @param buttonType The type of navigation button to be shown.
     */
    public void setNavigationButtonType(@NavigationButtonType int buttonType) {
        // TODO(twellington): Return early if the navigation button type and tint hasn't changed.
        if (!mIsTablet) return;

        mMediator.setNavigationButtonType(buttonType);

        ImageView navigationButton = mStatusView.getNavigationButton();
        if (navigationButton.getVisibility() != View.VISIBLE) {
            navigationButton.setVisibility(View.VISIBLE);
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
