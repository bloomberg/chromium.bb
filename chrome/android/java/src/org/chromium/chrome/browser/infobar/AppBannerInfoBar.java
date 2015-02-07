// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.banners.AppData;

/**
 * Infobar informing the user about an app related to this page.
 */
public class AppBannerInfoBar extends ConfirmInfoBar implements View.OnClickListener {
    // Installation states.
    public static final int INSTALL_STATE_NOT_INSTALLED = 0;
    public static final int INSTALL_STATE_INSTALLING = 1;
    public static final int INSTALL_STATE_INSTALLED = 2;

    // Views composing the infobar.
    private Button mButton;
    private View mTitleView;
    private View mIconView;

    private final String mAppTitle;

    // Data for native app installs.
    private final AppData mAppData;
    private int mInstallState;

    // Data for web app installs.
    private final String mAppUrl;

    // Banner for native apps.
    private AppBannerInfoBar(long nativeInfoBar, String appTitle, Bitmap iconBitmap, AppData data) {
        super(nativeInfoBar, null, 0, iconBitmap, appTitle, null, data.installButtonText(), null);
        mAppTitle = appTitle;
        mAppData = data;
        mAppUrl = null;
        mInstallState = INSTALL_STATE_NOT_INSTALLED;
    }

    // Banner for web apps.
    private AppBannerInfoBar(long nativeInfoBar, String appTitle, Bitmap iconBitmap, String url) {
        super(nativeInfoBar, null, 0, iconBitmap, appTitle, null, getAddToHomescreenText(), null);
        mAppTitle = appTitle;
        mAppData = null;
        mAppUrl = url;
        mInstallState = INSTALL_STATE_NOT_INSTALLED;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        if (mAppUrl != null) {
            TextView url = new TextView(layout.getContext());
            url.setText(mAppUrl);
            layout.setCustomContent(url);
        }

        super.createContent(layout);

        mButton = (Button) layout.findViewById(R.id.button_primary);
        mTitleView = layout.findViewById(R.id.infobar_message);

        // TODO(dfalcantara): Grab the icon from the layout.
        // mIconView = layout.findViewById(R.id.infobar_icon);

        assert mButton != null && mTitleView != null;

        // Set up accessibility text.
        Context context = getContext();
        if (mAppData != null) {
            layout.setContentDescription(context.getString(
                    R.string.app_banner_view_native_app_accessibility, mAppTitle,
                    mAppData.rating()));
        } else {
            layout.setContentDescription(context.getString(
                    R.string.app_banner_view_web_app_accessibility, mAppTitle,
                    mAppUrl));
        }

        // Set up clicking on the controls to bring up the app details.
        mTitleView.setOnClickListener(this);
        if (mIconView != null) mIconView.setOnClickListener(this);
    }

    @Override
    public void onButtonClicked(boolean isPrimaryButton) {
        if (mInstallState == INSTALL_STATE_INSTALLING) {
            setControlsEnabled(true);
            updateButton();
            return;
        }
        super.onButtonClicked(isPrimaryButton);
    }

    @CalledByNative
    public void onInstallStateChanged(int newState) {
        setControlsEnabled(true);
        mInstallState = newState;
        updateButton();
    }

    private void updateButton() {
        String text;
        String accessibilityText = null;
        boolean enabled = true;
        if (mInstallState == INSTALL_STATE_NOT_INSTALLED) {
            text = mAppData.installButtonText();
            accessibilityText =
                    getContext().getString(R.string.app_banner_install_accessibility, text);
        } else if (mInstallState == INSTALL_STATE_INSTALLING) {
            text = getContext().getString(R.string.app_banner_installing);
            enabled = false;
        } else {
            text = getContext().getString(R.string.app_banner_open);
        }

        mButton.setText(text);
        mButton.setContentDescription(accessibilityText);
        mButton.setEnabled(enabled);
    }

    @Override
    public void onClick(View v) {
        if (v == mTitleView || v == mIconView) onLinkClicked();
    }

    private static String getAddToHomescreenText() {
        return ApplicationStatus.getApplicationContext().getString(R.string.menu_add_to_homescreen);
    }

    @CalledByNative
    private static InfoBar createNativeAppInfoBar(
            long nativeInfoBar, String appTitle, Bitmap iconBitmap, AppData appData) {
        return new AppBannerInfoBar(nativeInfoBar, appTitle, iconBitmap, appData);
    }

    @CalledByNative
    private static InfoBar createWebAppInfoBar(
            long nativeInfoBar, String appTitle, Bitmap iconBitmap, String url) {
        return new AppBannerInfoBar(nativeInfoBar, appTitle, iconBitmap, url);
    }
}
