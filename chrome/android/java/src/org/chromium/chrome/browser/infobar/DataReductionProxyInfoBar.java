// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import org.chromium.content_public.browser.WebContents;

/**
 * Generates an InfoBar for the data reduction proxy that contains a message and a link to the
 * data reduction proxy settings menu.
 */
public class DataReductionProxyInfoBar extends ConfirmInfoBar {
    private static String sSettingsClassName;
    private static String sDataReductionProxySettingsClassName;
    private static String sTitle;
    private static String sLinkText;

    /**
     * Launch a data reduction proxy {@link InfoBar} with the specified title and link
     * text. Clicking the link will open the specified settings page.
     * @param webContents The {@link WebContents} in which to open the {@link InfoBar}.
     * @param settingsClassName The settings class to open.
     * @param drpSettingsClassName The {@link PreferenceActivity} fragment to show.
     * @param title The text to display in the {@link InfoBar}.
     * @param linkText The text to display in the link in the {@link InfoBar}.
     * @param linkUrl The URL to be loaded when the link text is clicked.
     */
    public static void launch(WebContents webContents,
            String settingsClassName,
            String drpSettingsClassName,
            String title,
            String linkText,
            String linkUrl) {
        sSettingsClassName = settingsClassName;
        sDataReductionProxySettingsClassName = drpSettingsClassName;
        sTitle = title;
        sLinkText = linkText;
        DataReductionProxyInfoBarDelegate.launch(webContents, linkUrl);
    }

    /**
     * Callers should now pass a linkUrl, which is loaded when the user clicks on linkText. See
     * {@link #launch} above. See http://crbug.com/383988.
     */
    @Deprecated
    public static void launch(WebContents webContents,
            String settingsClassName,
            String drpSettingsClassName,
            String title,
            String linkText) {
        launch(webContents, settingsClassName, drpSettingsClassName, title, linkText, "");
    }

    DataReductionProxyInfoBar(long nativeInfoBar, int iconDrawableId) {
        super(nativeInfoBar, null, iconDrawableId, sTitle,
                sLinkText, null, null);
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        layout.setButtons(sLinkText, null);
    }

    @Override
    public void onButtonClicked(boolean isPrimaryButton) {
        if (!isPrimaryButton) return;
        onLinkClicked();
    }
}
