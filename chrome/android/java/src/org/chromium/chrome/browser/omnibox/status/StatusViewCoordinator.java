// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.support.annotation.DrawableRes;
import android.support.annotation.IntDef;
import android.view.View;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.page_info.PageInfoController;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A component for displaying a status icon (e.g. security icon or navigation icon) and optional
 * verbose status text.
 */
public class StatusViewCoordinator implements View.OnClickListener {
    /**
     * Specifies the types of buttons shown to signify different types of navigation elements.
     */
    @IntDef({NavigationButtonType.PAGE, NavigationButtonType.MAGNIFIER, NavigationButtonType.EMPTY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface NavigationButtonType {
        int PAGE = 0;
        int MAGNIFIER = 1;
        int EMPTY = 2;
    }

    private final StatusView mStatusView;
    private final StatusMediator mMediator;
    private final PropertyModel mModel;
    private final boolean mIsTablet;

    private ToolbarDataProvider mToolbarDataProvider;
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
                         .with(StatusProperties.STATUS_ICON_TINT_RES,
                                 R.color.locationbar_status_separator_color)
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
    }

    /**
     * Provides data and state for the toolbar component.
     * @param toolbarDataProvider The data provider.
     */
    public void setToolbarDataProvider(ToolbarDataProvider toolbarDataProvider) {
        mToolbarDataProvider = toolbarDataProvider;
    }

    /**
     * Signals that native initialization has completed.
     */
    public void onNativeInitialized() {
        mMediator.setStatusClickListener(this);
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
        updateStatusIcon();
    }

    /**
     * Updates the security icon displayed in the LocationBar.
     */
    public void updateStatusIcon() {
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
        return mMediator.testIsSecurityButtonShown();
    }

    /**
     * @return The ID of the drawable currently shown in the security icon.
     */
    @VisibleForTesting
    @DrawableRes
    public int getSecurityIconResourceId() {
        return mModel.get(StatusProperties.STATUS_ICON_RES);
    }

    /**
     * Sets the type of the current navigation type and updates the UI to match it.
     * @param buttonType The type of navigation button to be shown.
     */
    public void setNavigationButtonType(@NavigationButtonType int buttonType) {
        @DrawableRes
        int imageRes = 0;
        switch (buttonType) {
            case NavigationButtonType.PAGE:
                imageRes = R.drawable.ic_omnibox_page;
                break;
            case NavigationButtonType.MAGNIFIER:
                imageRes = R.drawable.omnibox_search;
                break;
            case NavigationButtonType.EMPTY:
                break;
            default:
                assert false : "Invalid navigation button type";
        }
        mMediator.setNavigationButtonType(imageRes);
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

    @Override
    public void onClick(View view) {
        if (mUrlHasFocus) return;

        // Get Activity from our managed view.
        // TODO(ender): turn this into a property accessible via shared model.
        Context context = view.getContext();
        if (context == null || !(context instanceof Activity)) return;

        if (!mToolbarDataProvider.hasTab()
                || mToolbarDataProvider.getTab().getWebContents() == null) {
            return;
        }

        PageInfoController.show((Activity) context, mToolbarDataProvider.getTab(), null,
                PageInfoController.OpenedFromSource.TOOLBAR);
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
